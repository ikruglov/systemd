/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "hashmap.h"
#include "install.h"
#include "json-util.h"
#include "manager.h"
#include "metrics.h"
#include "set.h"
#include "sd-json.h"
#include "sd-varlink.h"
#include "unit.h"
#include "unit-def.h"
#include "varlink-metrics.h"

static int unit_states_total_build_json_one(
                sd_varlink *link,
                UnitActiveState state,
                unsigned count,
                bool more) {
        int r;
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

        assert(link);

        r = JSON_BUILD_METRIC(
                v,
                IO_SYSTEMD_MANAGER_UNIT_STATES_TOTAL,
                count,
                JSON_BUILD_METRIC_FIELD("state", unit_active_state_to_string(state)));
        if (r < 0)
                return r;

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

static int unit_types_total_build_json_one(sd_varlink *link, Manager *manager, UnitType *type, bool more) {
        int r;
        unsigned count = 0;
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

        assert(link);
        assert(manager);
        assert(type);

        // TODO: This needs a rework/improvement
        LIST_FOREACH(units_by_type, _u, manager->units_by_type[*type])
                count++;

        r = JSON_BUILD_METRIC(
                v,
                IO_SYSTEMD_MANAGER_UNIT_TYPES_TOTAL,
                count,
                JSON_BUILD_METRIC_FIELD("type", unit_type_to_string(*type)));
        if (r < 0)
                return r;

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

static int unit_state_build_json_one(sd_varlink *link, Unit *unit, bool more) {
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
        int r;

        assert(link);
        assert(unit);

        r = sd_json_buildo(
                &v,
                JSON_BUILD_METRIC_NAME(IO_SYSTEMD_MANAGER_UNIT_STATE),
                JSON_BUILD_METRIC_VALUE(0), /* 0 is a placeholder and has no meaning */
                JSON_BUILD_METRIC_OBJECT(unit->id),
                JSON_BUILD_METRIC_FIELDS(
                        JSON_BUILD_METRIC_FIELD5(
                                "state",
                                unit_active_state_to_string(unit_active_state(unit)),
                                "load_state",
                                unit_load_state_to_string(unit->load_state),
                                "sub_state",
                                unit_sub_state_to_string(unit),
                                "freezer_state",
                                freezer_state_to_string(unit->freezer_state),
                                "unit_file_state",
                                unit_file_state_to_string(unit_get_unit_file_state(unit)))));

        if (r < 0)
                return r;

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

static int unit_types_total_build_json(sd_varlink *link, void *userdata, bool more) {
        int r;
        Manager *manager = ASSERT_PTR(userdata);

        assert(link);
        assert(manager);

        UnitType *t, *previous_type = NULL;
        for (int i = 0; i < _UNIT_TYPE_MAX; i++) {
                UnitType type = (UnitType) i;
                t = &type;

                if (previous_type) {
                        r = unit_types_total_build_json_one(link, manager, previous_type, more);
                        if (r < 0)
                                return r;
                }

                previous_type = t;
       }

        if (previous_type) {
                r = unit_types_total_build_json_one(link, manager, previous_type, more);
                if (r < 0)
                        return r;
        } else
                return sd_varlink_error(link, VARLINK_ERROR_METRICS_NO_SUCH_METRIC, NULL);

        return 0;
}

static int unit_states_total_build_json(sd_varlink *link, void *userdata, bool more) {
        int r;
        Manager *manager = ASSERT_PTR(userdata);
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

        assert(link);

        r = unit_states_total_build_json_one(
                link,
                UNIT_ACTIVE,
                hashmap_size(manager->units),
                more);
        if (r < 0)
                return r;

        return unit_states_total_build_json_one(
                link,
                UNIT_FAILED,
                set_size(manager->failed_units),
                more);
}

static int unit_state_build_json(sd_varlink *link, void *userdata, bool more) {
        int r;
        const char *k;
        Unit *u, *previous = NULL;
        Manager *manager = ASSERT_PTR(userdata);

        HASHMAP_FOREACH_KEY(u, k, manager->units) {
                /* ignore aliases */
                if (k != u->id)
                        continue;

                if (previous) {
                        r = unit_state_build_json_one(link, previous, /* more = */ true);
                        if (r < 0)
                                return r;
                }

                previous = u;
        }

        if (previous)
                return unit_state_build_json_one(link, previous, more);

        return sd_varlink_error(link, VARLINK_ERROR_METRICS_NO_SUCH_METRIC, NULL);
}

const metrics_vtable vtable[] = {
        METRICS_VTABLE_START(),
        METRICS_FAMILY(
                IO_SYSTEMD_MANAGER_UNIT_STATES_TOTAL,
                "Total counts of units of different states",
                METRIC_GAUGE,
                unit_states_total_build_json),
        METRICS_FAMILY(
                IO_SYSTEMD_MANAGER_UNIT_TYPES_TOTAL,
                "Total counts of units of different types",
                METRIC_GAUGE,
                unit_types_total_build_json),
        METRICS_FAMILY(
                IO_SYSTEMD_MANAGER_UNIT_STATE,
                "Per unit metrics",
                METRIC_GAUGE,
                unit_state_build_json),

        METRICS_VTABLE_END
};

static int vtable_list_metrics(sd_varlink *link, void *userdata) {
        const metrics_vtable *i;
        bool more = true;
        int r;

        for(i = vtable; i->type != _METRICS_VTABLE_END; i++) {
                if (i->type != _METRICS_FAMILY)
                        continue;
                if ((i+1)->type == _METRICS_VTABLE_END)
                        more = false;
                r = i->x.metric_family.get(link, userdata, more);
                if (r < 0)
                        return r;
        }

        return 0;
}

int vl_method_list(
                sd_varlink *link,
                sd_json_variant *parameters,
                sd_varlink_method_flags_t flags,
                void *userdata) {
        int r;

        assert(link);
        assert(parameters);

        r = sd_varlink_dispatch(link, parameters, /* dispatch_table= */ NULL, /* userdata= */ NULL);
        if (r != 0)
                return r;

        if (!FLAGS_SET(flags, SD_VARLINK_METHOD_MORE))
                return sd_varlink_error(link, SD_VARLINK_ERROR_EXPECTED_MORE, NULL);

        r = vtable_list_metrics(link, userdata);
        if (r < 0)
                return r;

        return 0;
}
