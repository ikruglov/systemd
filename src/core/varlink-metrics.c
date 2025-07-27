/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <assert.h>
#include <string.h>

#include "hashmap.h"
#include "install.h"
#include "json-util.h"
#include "manager.h"
#include "set.h"
#include "sd-json.h"
#include "sd-varlink.h"
#include "unit.h"
#include "unit-def.h"
#include "varlink-metrics.h"

static int list_units_count_one(sd_varlink *link, Manager *m, const char *state, bool more) {
        int r, count;
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

        assert(m);
        assert(state);

        if (0 == strcmp(state, "active"))
                count = hashmap_size(m->units);
        else if (0 == strcmp(state, "failed"))
                count = set_size(m->failed_units);
        else
                return sd_varlink_error(NULL, VARLINK_ERROR_METRICS_NO_SUCH_METRIC, NULL);

        r = sd_json_buildo(
                &v,
                SD_JSON_BUILD_PAIR_STRING("name", "units"),
                SD_JSON_BUILD_PAIR_UNSIGNED("value", count),
                SD_JSON_BUILD_PAIR(
                        "fields",
                        SD_JSON_BUILD_OBJECT(SD_JSON_BUILD_PAIR_STRING("state", state))));
        if (r < 0)
                return log_error_errno(r, "Failed to list unit status: %m");

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

static int list_unit_type_one(sd_varlink *link, Manager *m, UnitType *t, bool more) {
        int r;
        unsigned count = 0;
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

        assert(link);
        assert(m);
        assert(t);

        // TODO: This needs a rework/improvement
        LIST_FOREACH(units_by_type, _u, m->units_by_type[*t])
                count++;

        r = sd_json_buildo(
                &v,
                SD_JSON_BUILD_PAIR_STRING("name", "units"),
                SD_JSON_BUILD_PAIR_UNSIGNED("value", count),
                SD_JSON_BUILD_PAIR(
                        "fields",
                        SD_JSON_BUILD_OBJECT(SD_JSON_BUILD_PAIR_STRING("type", unit_type_to_string(*t)))));
        if (r < 0)
                return r;

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

static int list_per_unit_metrics_one(sd_varlink *link, Unit *unit, bool more) {
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
        int r;

        assert(link);
        assert(unit);

        r = sd_json_buildo(
                &v,
                SD_JSON_BUILD_PAIR_STRING("name", IO_SYSTEMD_MANAGER"unit_state"),
                SD_JSON_BUILD_PAIR_UNSIGNED("value", 0), /* 0 is a placeholder and has no meaning */
                SD_JSON_BUILD_PAIR(
                        "fields",
                        SD_JSON_BUILD_OBJECT(
                                JSON_BUILD_PAIR_STRING_NON_EMPTY("unit", unit->id),
                                JSON_BUILD_PAIR_STRING_NON_EMPTY("state", unit_active_state_to_string(unit_active_state(unit))),
                                JSON_BUILD_PAIR_STRING_NON_EMPTY("load_state", unit_load_state_to_string(unit->load_state)),
                                JSON_BUILD_PAIR_STRING_NON_EMPTY("sub_state", unit_sub_state_to_string(unit)),
                                JSON_BUILD_PAIR_STRING_NON_EMPTY("freezer_state", freezer_state_to_string(unit->freezer_state)),
                                JSON_BUILD_PAIR_STRING_NON_EMPTY("unit_file_state", unit_file_state_to_string(unit_get_unit_file_state(unit))))));

        if (r < 0)
                return r;

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

int vl_method_list(sd_varlink *link, sd_json_variant *parameters, sd_varlink_method_flags_t flags, void *userdata) {
        Manager *manager = ASSERT_PTR(userdata);
        int r;

        assert(link);
        assert(parameters);

        r = sd_varlink_dispatch(link, parameters, /* dispatch_table= */ NULL, /* userdata= */ NULL);
        if (r != 0)
                return r;

        if (!FLAGS_SET(flags, SD_VARLINK_METHOD_MORE))
                return sd_varlink_error(link, SD_VARLINK_ERROR_EXPECTED_MORE, NULL);

        const char *states[] = {
                "active",
                "failed",
                NULL
        };

        for (int i = 0; states[i] != NULL; i++) {
                r = list_units_count_one(link, manager, states[i], /* more = */ true);
                if (r < 0)
                        return r;
        }

        UnitType *t, *previous_type = NULL;
        for (int i = 0; i < _UNIT_TYPE_MAX; i++) {
                UnitType type = (UnitType) i;
                t = &type;

                if (previous_type) {
                        r = list_unit_type_one(link, manager, previous_type, /* more = */ true);
                        if (r < 0)
                                return r;
                }

                previous_type = t;
       }

        if (previous_type) {
                r = list_unit_type_one(link, manager, previous_type, /* more = */ true);
                if (r < 0)
                        return r;
        } else
                return sd_varlink_error(link, VARLINK_ERROR_METRICS_NO_SUCH_METRIC, NULL);

        const char *k;
        Unit *u, *previous = NULL;
        HASHMAP_FOREACH_KEY(u, k, manager->units) {
                /* ignore aliases */
                if (k != u->id)
                        continue;

                if (previous) {
                        r = list_per_unit_metrics_one(link, previous, /* more = */ true);
                        if (r < 0)
                                return r;
                }

                previous = u;
        }

        if (previous)
                return list_per_unit_metrics_one(link, previous, /* more = */ false);

        return sd_varlink_error(link, VARLINK_ERROR_METRICS_NO_SUCH_METRIC, NULL);

}
