#include "varlink-io.systemd.Metrics.h"


static SD_VARLINK_DEFINE_METHOD(
               List,
               SD_VARLINK_FIELD_COMMENT("Metric name"),
               SD_VARLINK_DEFINE_OUTPUT(name, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
               SD_VARLINK_FIELD_COMMENT("Value of the metric"),
               SD_VARLINK_DEFINE_OUTPUT(value, SD_VARLINK_INT, 0),
               SD_VARLINK_FIELD_COMMENT("fields"),
               SD_VARLINK_DEFINE_OUTPUT(fields, SD_VARLINK_STRUCT, SD_VARLINK_NULLABLE));


SD_VARLINK_DEFINE_INTERFACE(
               io_systemd_Metrics,
               "io.systemd.Metrics",
               SD_VARLINK_INTERFACE_COMMENT("Metrics APIs"),
               SD_VARLINK_SYMBOL_COMMENT(
                               "Return key -> value dictionary of relevant metrics, optionally filtered by object."),
               &vl_method_List);

