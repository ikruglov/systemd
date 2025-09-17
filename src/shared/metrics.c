/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "json-util.h"
#include "log.h"
#include "metrics.h"
#include "string-table.h"
#include "strv.h"
#include "varlink-io.systemd.Metrics.h"
#include "varlink-serialize.h"
#include "varlink-util.h"

int metrics_setup_varlink_server(
                sd_varlink_server **server, /* in and out param */
                sd_varlink_server_flags_t flags,
                sd_event *event,
                sd_varlink_method_t vl_method_list_cb,
                sd_varlink_method_t vl_method_describe_cb,
                void *userdata) {
        _cleanup_(sd_varlink_server_unrefp) sd_varlink_server *s = NULL;
        int r;

        assert(server);
        assert(event);

        if (*server)
                return 0;

        r = varlink_server_new(&s, flags, userdata);
        if (r < 0)
                return log_debug_errno(r, "Failed to allocate varlink metrics server object: %m");

        r = sd_varlink_server_add_interface(s, &vl_interface_io_systemd_Metrics);
        if (r < 0)
                return log_debug_errno(r, "Failed to add varlink metrics interface to varlink server: %m");

        r = sd_varlink_server_bind_method_many(
                s,
                "io.systemd.Metrics.List", vl_method_list_cb,
                "io.systemd.Metrics.Describe", vl_method_describe_cb);
        if (r < 0)
                return log_debug_errno(r, "Failed to register varlink metrics methods: %m");

        r = sd_varlink_server_attach_event(s, event, SD_EVENT_PRIORITY_NORMAL);
        if (r < 0)
                return log_debug_errno(r, "Failed to attach varlink metrics connection to event loop: %m");

        *server = TAKE_PTR(s);
        return 0;
}

int metrics_listen_varlink_address(sd_varlink_server *server, const char *address) {
        int r;

        assert(server);
        assert(address);

        /* a new server will have empty list of addresses anyway */
        if (varlink_server_contains_socket(server, address))
                return 0;

        r = sd_varlink_server_listen_address(server, address, 0666 | SD_VARLINK_SERVER_MODE_MKDIR_0755);
        if (r < 0)
                return log_debug_errno(r, "Failed to bind to metrics varlink socket '%s': %m", address);

        return 0;
}

static const char * const metric_family_type_table[_METRIC_FAMILY_TYPE_MAX] = {
        [METRIC_FAMILY_TYPE_COUNTER] = "counter",
        [METRIC_FAMILY_TYPE_GAUGE]   = "gauge",
        [METRIC_FAMILY_TYPE_STRING]  = "string",
};

DEFINE_STRING_TABLE_LOOKUP_TO_STRING(metric_family_type, MetricFamilyType);

int metric_family_json_build(sd_json_variant **v, const MetricFamily *metric_family) {
        assert(metric_family);
        return sd_json_buildo(
                ASSERT_PTR(v),
                SD_JSON_BUILD_PAIR_STRING("name", metric_family->name),
                SD_JSON_BUILD_PAIR_STRING("description", metric_family->description),
                SD_JSON_BUILD_PAIR_STRING("type", metric_family_type_to_string(metric_family->type)));
}

static int metric_fields_build_json(sd_json_variant **ret, const char *name, void *userdata) {
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
        char **fields = ASSERT_PTR(userdata);
        int r;

        assert(ret);
        assert(name);

        if (strv_isempty(fields)) {
                *ret = NULL;
                return 0;
        }

        STRV_FOREACH_PAIR(fk, fv, fields) {
                r = sd_json_variant_merge_objectbo(&v, SD_JSON_BUILD_PAIR_STRING(*fk, *fv));
                if (r < 0)
                        return r;
        }

        *ret = TAKE_PTR(v);
        return 0;
}

static int metric_json_build_body(sd_json_variant **v, const char *name, const char *object, char **fields) {
        return sd_json_buildo(
                        ASSERT_PTR(v),
                        SD_JSON_BUILD_PAIR_STRING("name", ASSERT_PTR(name)),
                        JSON_BUILD_PAIR_STRING_NON_EMPTY("object", object),
                        JSON_BUILD_PAIR_CALLBACK_NON_NULL("fields", metric_fields_build_json, fields));
}

int metric_json_build_unsigned(sd_json_variant **v, const char *name, const char *object, uint64_t value, char **fields) {
        int r;
        r = metric_json_build_body(v, name, object, fields);
        if (r < 0)
                return r;

        return sd_json_variant_set_field_unsigned(v, "value", value);
}

int metric_json_build_integer(sd_json_variant **v, const char *name, const char *object, int64_t value, char **fields) {
        int r;
        r = metric_json_build_body(v, name, object, fields);
        if (r < 0)
                return r;

        return sd_json_variant_set_field_integer(v, "value", value);
}

int metric_json_build_string(sd_json_variant **v, const char *name, const char *object, const char *value, char **fields) {
        int r;
        r = metric_json_build_body(v, name, object, fields);
        if (r < 0)
                return r;

        return sd_json_variant_set_field_string(v, "value", value);
}
