/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "varlink-io.systemd.Metrics.h"

/* TODO Let's use this as a temporary solution */
static SD_VARLINK_DEFINE_STRUCT_TYPE(
                MetricFields,
                SD_VARLINK_FIELD_COMMENT("State of the unit. Applicable for X metrics"),
                SD_VARLINK_DEFINE_FIELD(state, SD_VARLINK_STRING, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_STRUCT_TYPE(
                Metric,
                SD_VARLINK_FIELD_COMMENT("Metric name"),
                SD_VARLINK_DEFINE_FIELD(name, SD_VARLINK_STRING, 0),
                SD_VARLINK_FIELD_COMMENT("Metric value"),
                SD_VARLINK_DEFINE_FIELD(value, SD_VARLINK_INT, 0),
                SD_VARLINK_FIELD_COMMENT("Object name"),
                SD_VARLINK_DEFINE_FIELD(object, SD_VARLINK_STRING, 0),
                SD_VARLINK_FIELD_COMMENT("Metric fields"),
                SD_VARLINK_DEFINE_OUTPUT_BY_TYPE(fields, MetricFields, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
                List,
                // SD_VARLINK_SUPPORTS_MORE, TODO this will be necessary for streaming
                SD_VARLINK_DEFINE_OUTPUT_BY_TYPE(metrics, Metric, 0));

SD_VARLINK_DEFINE_INTERFACE(
               io_systemd_Metrics,
               "io.systemd.Metrics",
               SD_VARLINK_INTERFACE_COMMENT("Metrics APIs"),
               SD_VARLINK_SYMBOL_COMMENT("Method to get list of metrics"),
               &vl_method_List,
               SD_VARLINK_SYMBOL_COMMENT("The metric fields object"),
               &vl_type_MetricFields,
               SD_VARLINK_SYMBOL_COMMENT("The metric object"),
               &vl_type_Metric);
