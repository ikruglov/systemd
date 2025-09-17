/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "forward.h"

#define VARLINK_ERROR_METRICS_NO_SUCH_METRIC "io.systemd.Metrics.NoSuchMetric"

#define IO_SYSTEMD_MANAGER "io.systemd.Manager."
#define UNIT_STATES_TOTAL "unit_states_total"
#define UNIT_TYPES_TOTAL "unit_types_total"
#define UNIT_STATE "unit_state"
#define IO_SYSTEMD_MANAGER_UNIT_STATES_TOTAL IO_SYSTEMD_MANAGER UNIT_STATES_TOTAL
#define IO_SYSTEMD_MANAGER_UNIT_TYPES_TOTAL IO_SYSTEMD_MANAGER UNIT_TYPES_TOTAL
#define IO_SYSTEMD_MANAGER_UNIT_STATE IO_SYSTEMD_MANAGER UNIT_STATE

int vl_method_list(sd_varlink *link, sd_json_variant *parameters, sd_varlink_method_flags_t flags, void *userdata);

int vl_method_describe(
                sd_varlink *link,
                sd_json_variant *parameters,
                sd_varlink_method_flags_t flags,
                void *userdata);
