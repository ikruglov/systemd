/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "forward.h"

#define VARLINK_ERROR_METRICS_NO_SUCH_METRIC "io.systemd.Metrics.NoSuchMetric"
#define IO_SYSTEMD_MANAGER "io.systemd.Manager."

int vl_method_list(sd_varlink *link, sd_json_variant *parameters, sd_varlink_method_flags_t flags, void *userdata);
