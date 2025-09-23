/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "hashmap.h"
#include "install.h"
#include "json-util.h"
#include "manager.h"
#include "metrics.h"
#include "service.h"
#include "set.h"
#include "sd-json.h"
#include "sd-varlink.h"
#include "unit.h"
#include "unit-def.h"
#include "varlink-metrics.h"

static int units_by_state_total_build_json_one(
                sd_varlink *link,
                UnitActiveState state,
                unsigned count,
                bool more) {
        int r;
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

        assert(link);

        r = METRIC_JSON_BUILD_UNSIGNED(
                &v,
                METRIC_IO_SYSTEMD_MANAGER_UNITS_BY_STATE_TOTAL,
                /* object= */ NULL,
                count,
                /* fields */ "state",
                unit_active_state_to_string(state));
        if (r < 0)
                return r;

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

static int units_by_state_total_build_json(sd_varlink *link, void *userdata, bool more) {
        int r;
        Manager *manager = ASSERT_PTR(userdata);

        assert(link);

        r = units_by_state_total_build_json_one(
                link,
                UNIT_ACTIVE,
                hashmap_size(manager->units),
                more);

        if (r < 0)
                return r;

        return units_by_state_total_build_json_one(
                link,
                UNIT_FAILED,
                set_size(manager->failed_units),
                more);
}

static int units_by_type_total_build_json_one(sd_varlink *link, Manager *manager, UnitType *type, bool more) {
        int r;
        unsigned count = 0;
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

        assert(link);
        assert(manager);
        assert(type);

        // TODO: This needs a rework/improvement
        LIST_FOREACH(units_by_type, _u, manager->units_by_type[*type])
                count++;

        r = METRIC_JSON_BUILD_UNSIGNED(
                &v,
                METRIC_IO_SYSTEMD_MANAGER_UNITS_BY_TYPE_TOTAL,
                /* object= */ NULL,
                count,
                /* fields */ "type",
                unit_type_to_string(*type));
        if (r < 0)
                return r;

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

static int units_by_type_total_build_json(sd_varlink *link, void *userdata, bool more) {
        int r;
        Manager *manager = ASSERT_PTR(userdata);

        assert(link);

        UnitType *t, *previous_type = NULL;
        UnitType type;
        for (int i = 0; i < _UNIT_TYPE_MAX; i++) {
                type = (UnitType) i;
                t = &type;

                if (previous_type) {
                        r = units_by_type_total_build_json_one(link, manager, previous_type, more);
                        if (r < 0)
                                return r;
                }

                previous_type = t;
        }

        return units_by_type_total_build_json_one(link, manager, previous_type, more);
}

static int unit_active_state_build_json_one(sd_varlink *link, Unit *unit, bool more) {
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
        int r;

        assert(link);
        assert(unit);

        r = METRIC_JSON_BUILD_STRING(
                &v,
                METRIC_IO_SYSTEMD_MANAGER_UNIT_ACTIVE_STATE,
                unit->id,
                unit_active_state_to_string(unit_active_state(unit)),
                /* fields */ NULL);
        if (r < 0)
                return r;

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

static int unit_active_state_build_json(sd_varlink *link, void *userdata, bool more) {
        int r;
        const char *k;
        Unit *u, *previous = NULL;
        Manager *manager = ASSERT_PTR(userdata);

        assert(link);

        HASHMAP_FOREACH_KEY(u, k, manager->units) {
                /* ignore aliases */
                if (k != u->id)
                        continue;
                if (previous) {
                        r = unit_active_state_build_json_one(link, previous, /* more = */ true);
                        if (r < 0)
                                return r;
                }

                previous = u;
        }

        if (previous)
                return unit_active_state_build_json_one(link, previous, more);

        return sd_varlink_error(link, VARLINK_ERROR_METRICS_NO_SUCH_METRIC, NULL);
}

static int unit_load_state_build_json_one(sd_varlink *link, Unit *unit, bool more) {
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
        int r;

        assert(link);
        assert(unit);

        r = METRIC_JSON_BUILD_STRING(
                &v,
                METRIC_IO_SYSTEMD_MANAGER_UNIT_LOAD_STATE,
                unit->id,
                unit_load_state_to_string(unit->load_state),
                /* fields */ NULL);
        if (r < 0)
                return r;

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

static int unit_load_state_build_json(sd_varlink *link, void *userdata, bool more) {
        int r;
        const char *k;
        Unit *u, *previous = NULL;
        Manager *manager = ASSERT_PTR(userdata);

        assert(link);

        HASHMAP_FOREACH_KEY(u, k, manager->units) {
                /* ignore aliases */
                if (k != u->id)
                        continue;
                if (previous) {
                        r = unit_load_state_build_json_one(link, previous, /* more = */ true);
                        if (r < 0)
                                return r;
                }

                previous = u;
        }

        if (previous)
                return unit_load_state_build_json_one(link, previous, more);

        return sd_varlink_error(link, VARLINK_ERROR_METRICS_NO_SUCH_METRIC, NULL);
}

static int nrestarts_build_json_one(sd_varlink *link, Unit *unit, bool more) {
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
        int r;

        assert(link);
        assert(unit);

        Service *service = SERVICE(unit);

        r = METRIC_JSON_BUILD_UNSIGNED(
                &v,
                METRIC_IO_SYSTEMD_MANAGER_SERVICE_NRESTARTS,
                service->meta.id,
                service->n_restarts,
                /* fields */ NULL);
        if (r < 0)
                return r;

        if (more)
                return sd_varlink_notify(link, v);

        return sd_varlink_reply(link, v);
}

static int nrestarts_build_json(sd_varlink *link, void *userdata, bool more) {
        Unit *previous = NULL;
        Manager *manager = ASSERT_PTR(userdata);

        assert(link);

        LIST_FOREACH(units_by_type, u, manager->units_by_type[UNIT_SERVICE]) {
                if (previous) {
                        int r = nrestarts_build_json_one(link, previous, /* more = */ true);
                        if (r < 0)
                                return r;
                }
                previous = u;
        }

        if (previous)
                return nrestarts_build_json_one(link, previous, more);

        return sd_varlink_error(link, VARLINK_ERROR_METRICS_NO_SUCH_METRIC, NULL);
}

const MetricFamily metric_family_table[] = {
        METRIC_FAMILY(
                METRIC_IO_SYSTEMD_MANAGER_UNITS_BY_STATE_TOTAL,
                "Total counts of units of different states",
                METRIC_FAMILY_TYPE_GAUGE,
                units_by_state_total_build_json),
        METRIC_FAMILY(
                METRIC_IO_SYSTEMD_MANAGER_UNITS_BY_TYPE_TOTAL,
                "Total counts of units of different types",
                METRIC_FAMILY_TYPE_GAUGE,
                units_by_type_total_build_json),
        METRIC_FAMILY(
                METRIC_IO_SYSTEMD_MANAGER_UNIT_ACTIVE_STATE,
                "Per unit metric: active state",
                METRIC_FAMILY_TYPE_STRING,
                unit_active_state_build_json),
        METRIC_FAMILY(
                METRIC_IO_SYSTEMD_MANAGER_UNIT_LOAD_STATE,
                "Per unit metric: load state",
                METRIC_FAMILY_TYPE_STRING,
                unit_load_state_build_json),
        METRIC_FAMILY(
                METRIC_IO_SYSTEMD_MANAGER_SERVICE_NRESTARTS,
                "Per service metric: n_restarts state",
                METRIC_FAMILY_TYPE_GAUGE,
                nrestarts_build_json),
        {},
};

int vl_method_describe(sd_varlink *link, sd_json_variant *parameters, sd_varlink_method_flags_t flags, void *userdata) {
        return metric_method_describe(metric_family_table, link, parameters, flags, userdata);
}

int vl_method_list(sd_varlink *link, sd_json_variant *parameters, sd_varlink_method_flags_t flags, void *userdata) {
        return metric_method_list(metric_family_table, link, parameters, flags, userdata);
}
