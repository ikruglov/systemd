#include <assert.h>

#include "alloc-util.h"
#include "dirent-util.h"
#include "fd-util.h"
#include "log.h"
#include "metrics.h"
#include "varlink-serialize.h"
#include "varlink-io.systemd.Metrics.h"
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

const char* metric_type_to_string(MetricType t) {
        switch (t) {
            case METRIC_COUNTER:
                return "Counter";
            case METRIC_GAUGE:
                return "Gauge";
            default:
                return "UNKNOWN";
        }
}

static int metrics_on_query_reply(
        sd_varlink *link,
        sd_json_variant *parameters,
        const char *error,
        sd_varlink_reply_flags_t flags,
        void *userdata) {

        int *ret = ASSERT_PTR(userdata), r;

        assert(link);

        if (error) {
                /* If we can translate this to an errno, let's print that as errno and return it, otherwise, return a generic error code */
                r = sd_varlink_error_to_errno(error, parameters);
                if (r != -EBADR)
                        *ret = log_error_errno(r, "Method call failed: %m");
                else
                        r = *ret = log_error_errno(SYNTHETIC_ERRNO(EBADE), "Method call failed: %s", error);
        } else
                r = 0;

        sd_json_variant_dump(parameters, SD_JSON_FORMAT_PRETTY_AUTO|SD_JSON_FORMAT_COLOR_AUTO, stdout, NULL);

        return r;
}

static int metrics_call(const char *path) {
        _cleanup_(sd_varlink_unrefp) sd_varlink *vl = NULL;
        int r;

        assert(path);

        r = sd_varlink_connect_address(&vl, path);
        if (r < 0)
                return log_debug_errno(r, "Unable to connect to %s: %m", path);

        int ret = 0;
        sd_varlink_set_userdata(vl, &ret);

        r = sd_varlink_bind_reply(vl, metrics_on_query_reply);
        if (r < 0)
                return log_debug_errno(r, "Failed to bind reply callback: %m");

        sd_varlink_observe(vl, "io.systemd.Metrics.List", /* parameter */ NULL);
        if (r < 0)
                return log_debug_errno(r, "Failed to invoke varlink method: %m");

        for (;;) {
                r = sd_varlink_is_idle(vl);

                if (r < 0)
                        return log_error_errno(r, "Failed to check if varlink connection is idle: %m");
                if (r > 0) {
                        break;
                }

                r = sd_varlink_process(vl);
                if (r < 0)
                        return log_error_errno(r, "Failed to process varlink connection: %m");
                if (r != 0)
                        continue;

                r = sd_varlink_wait(vl, USEC_INFINITY);
                if (r < 0)
                        return log_error_errno(r, "Failed to wait for varlink connection events: %m");
        }

        return ret;
}

static int metrics_start_query(void) {
        _cleanup_closedir_ DIR *d = NULL;
        int r;

        d = opendir("/run/systemd/metrics/");
        if (!d) {
                if (errno == ENOENT)
                        return -ESRCH;

                return -errno;
        }

        FOREACH_DIRENT(de, d, return -errno) {
                _cleanup_free_ char *p = NULL;

                p = path_join("/run/systemd/metrics/", de->d_name);
                if (!p)
                        return -ENOMEM;

                r = metrics_call(p);
                if (r < 0)
                        return r;
        }

        return 0;
}

int metrics_query_all(sd_json_variant **ret) {
        int r;

        assert(ret);

        r = metrics_start_query();
        if (r < 0)
                return r;

        return 0;
}
