#include "varlink-io.systemd.Metrics.h"

static SD_VARLINK_DEFINE_STRUCT_TYPE(
                MetricsStruct,
                SD_VARLINK_FIELD_COMMENT("Metric family name"),
                SD_VARLINK_DEFINE_FIELD(name, SD_VARLINK_STRING, 0),
                SD_VARLINK_FIELD_COMMENT("Value of the metric"),
                SD_VARLINK_DEFINE_FIELD(value, SD_VARLINK_INT, 0),
                SD_VARLINK_FIELD_COMMENT("fields"),
                SD_VARLINK_DEFINE_FIELD(fields, SD_VARLINK_STRUCT, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
                List,
                SD_VARLINK_DEFINE_OUTPUT_BY_TYPE(metrics, MetricsStruct, 0));

SD_VARLINK_DEFINE_INTERFACE(
               io_systemd_Metrics,
               "io.systemd.Metrics",
               SD_VARLINK_INTERFACE_COMMENT("Metrics APIs"),
               SD_VARLINK_SYMBOL_COMMENT(
                               "APIs for getting relevant metrics, optionally filtered by object."),
               &vl_method_List,
               SD_VARLINK_SYMBOL_COMMENT("The metrics object"),
               &vl_type_MetricsStruct);
