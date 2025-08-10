/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "sd-varlink-idl.h"

#include "varlink-io.systemd.Metrics.h"

/* TODO Let's use this as a temporary solution */
static SD_VARLINK_DEFINE_STRUCT_TYPE(
                MetricFields,
                SD_VARLINK_FIELD_COMMENT("State of the unit. Applicable for X metrics"),
                SD_VARLINK_DEFINE_FIELD(state, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_FIELD(load_state, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_FIELD(sub_state, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_FIELD(freezer_state, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_FIELD(unit_file_state, SD_VARLINK_STRING, SD_VARLINK_NULLABLE));

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

static SD_VARLINK_DEFINE_STRUCT_TYPE(
                MetricMetadata,
                SD_VARLINK_FIELD_COMMENT("Metric name"),
                SD_VARLINK_DEFINE_FIELD(name, SD_VARLINK_STRING, 0),
                SD_VARLINK_FIELD_COMMENT("Metric description"),
                SD_VARLINK_DEFINE_FIELD(value, SD_VARLINK_STRING, 0),
                SD_VARLINK_FIELD_COMMENT("Metric type"),
                SD_VARLINK_DEFINE_FIELD(type, SD_VARLINK_STRING, 0));

static SD_VARLINK_DEFINE_ERROR(NoSuchMetric);

static SD_VARLINK_DEFINE_METHOD_FULL(
                List,
                SD_VARLINK_SUPPORTS_MORE,
                SD_VARLINK_DEFINE_OUTPUT_BY_TYPE(metrics, Metric, 0));

static SD_VARLINK_DEFINE_METHOD_FULL(
                Describe,
                SD_VARLINK_SUPPORTS_MORE,
                SD_VARLINK_DEFINE_OUTPUT_BY_TYPE(metadata, MetricMetadata, 0));

SD_VARLINK_DEFINE_INTERFACE(
               io_systemd_Metrics,
               "io.systemd.Metrics",
               SD_VARLINK_INTERFACE_COMMENT("Metrics APIs"),
               SD_VARLINK_SYMBOL_COMMENT("Method to get list of metrics"),
               &vl_method_List,
               SD_VARLINK_SYMBOL_COMMENT("Method to get the metadata of metric families"),
               &vl_method_Describe,
               SD_VARLINK_SYMBOL_COMMENT("The metric fields object"),
               &vl_type_MetricFields,
               SD_VARLINK_SYMBOL_COMMENT("The metric object"),
               &vl_type_Metric,
               SD_VARLINK_SYMBOL_COMMENT("The metric family metadata object"),
               &vl_type_MetricMetadata,
               SD_VARLINK_SYMBOL_COMMENT("No such metric found"),
               &vl_error_NoSuchMetric);
