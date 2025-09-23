#pragma once

#include "sd-json.h"
#include "sd-varlink.h"

#include "forward.h"

typedef enum MetricFamilyType {
        METRIC_FAMILY_TYPE_COUNTER,
        METRIC_FAMILY_TYPE_GAUGE,
        METRIC_FAMILY_TYPE_STRING,
        _METRIC_FAMILY_TYPE_MAX,
        _METRIC_FAMILY_TYPE_INVALID = -EINVAL,
} MetricFamilyType;

typedef int (*metric_family_get_t) (sd_varlink *link, void *userdata, bool more);

typedef struct MetricFamily {
        const char *name;
        const char *description;
        MetricFamilyType type;
        metric_family_get_t cb;
} MetricFamily;

int metrics_setup_varlink_server(
                sd_varlink_server **server, /* in and out param */
                sd_varlink_server_flags_t flags,
                sd_event *event,
                sd_varlink_method_t vl_method_list_cb,
                sd_varlink_method_t vl_method_describe_cb,
                void *userdata);

int metrics_listen_varlink_address(sd_varlink_server *server, const char *address);
const char* metric_family_type_to_string(MetricFamilyType t) _const_;
int metric_family_json_build(sd_json_variant **v, const MetricFamily *metric_family);

#define METRIC_FAMILY(_name, _description, _type, _cb)  \
        {                                               \
                .name = _name,                          \
                .description = _description,            \
                .type = _type,                          \
                .cb = _cb,                              \
        }

#define METRIC_JSON_BUILD_UNSIGNED(_v, _name, _object, _value, ...) metric_json_build_unsigned(_v, _name, _object, _value, STRV_MAKE(__VA_ARGS__))
#define METRIC_JSON_BUILD_INTEGER(_v, _name, _object, _value, ...) metric_json_build_integer(_v, _name, _object, _value, STRV_MAKE(__VA_ARGS__))
#define METRIC_JSON_BUILD_STRING(_v, _name, _object, _value, ...) metric_json_build_string(_v, _name, _object, _value, STRV_MAKE(__VA_ARGS__))
int metric_json_build_unsigned(sd_json_variant **v, const char *name, const char *object, uint64_t value, char **fields);
int metric_json_build_integer(sd_json_variant **v, const char *name, const char *object, int64_t value, char **fields);
int metric_json_build_string(sd_json_variant **v, const char *name, const char *object, const char *value, char **fields);

int metric_method_describe(const MetricFamily metric_family_table[], sd_varlink *link, sd_json_variant *parameters, sd_varlink_method_flags_t flags, void *userdata);
int metric_method_list(const MetricFamily metric_family_table[], sd_varlink *link, sd_json_variant *parameters, sd_varlink_method_flags_t flags, void *userdata);
