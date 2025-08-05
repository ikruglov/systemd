#pragma once

#include "sd-varlink.h"

int metrics_setup_varlink_server(
                sd_varlink_server **server, /* in and out param */
                sd_varlink_server_flags_t flags,
                sd_event *event,
                sd_varlink_method_t vl_method_list_cb,
                void *userdata);

int metrics_listen_varlink_address(sd_varlink_server *server, const char *address);

typedef struct metrics_vtable metrics_vtable;

/* Callbacks */
typedef int (*metric_family_get_t) (sd_varlink *link, void *userdata, bool more);

enum {
        _METRICS_VTABLE_START             = '<',
        _METRICS_VTABLE_END               = '>',
        _METRICS_FAMILY                   = 'F',
};

typedef enum MetricType {
        METRIC_COUNTER,
        METRIC_GAUGE,
        METRIC_TYPE_MAX,
        METRIC_TYPE_INVALID = -EINVAL,
        METRIC_TYPE_ERRNO_MAX = -ERRNO_MAX, /* Ensure the whole errno range fits into this enum */
} MetricType;

const char* metric_type_to_string(MetricType i) _const_;
MetricType metric_type_from_string(const char *s) _pure_;

/* Note: unused areas in the metrics_vtable[] array must be initialized to 0. The structure contains an
 * embedded union, and the compiler is NOT required to initialize the unused areas of the union when the rest
 * of the structure is initialized. Normally the array is defined as read-only data, in which case the linker
 * places it in the BSS section, which is always fully initialized, so this is not a concern. But if the
 * array is created on the stack or on the heap, care must be taken to initialize the unused areas, for
 * example by first memsetting the whole region to zero before filling the data in. */

struct metrics_vtable {
        /* Please do not initialize this structure directly, use the
         * macros below instead */

        __extension__ uint8_t type:8;
        union {
                struct {
                        size_t element_size;
                } start;
                struct {
                        /* This field exists only to make sure we have something to initialize in
                         * SD_BUS_VTABLE_END in a way that is both compatible with pedantic versions of C and
                         * C++. It's unused otherwise. */
                        size_t _reserved;
                } end;
                struct {
                        const char *name;
                        const char *description;
                        MetricType type;
                        metric_family_get_t get;
                } metric_family;
        } x;
};

#define IO_SYSTEMD_MANAGER "io.systemd.Manager."
#define UNIT_STATES_TOTAL "unit_states_total"
#define UNIT_TYPES_TOTAL "unit_types_total"
#define UNIT_STATE "unit_state"
#define IO_SYSTEMD_MANAGER_UNIT_STATES_TOTAL IO_SYSTEMD_MANAGER UNIT_STATES_TOTAL
#define IO_SYSTEMD_MANAGER_UNIT_TYPES_TOTAL IO_SYSTEMD_MANAGER UNIT_TYPES_TOTAL
#define IO_SYSTEMD_MANAGER_UNIT_STATE IO_SYSTEMD_MANAGER UNIT_STATE

#define METRICS_VTABLE_START()                                          \
        {                                                               \
                .type = _METRICS_VTABLE_START,                          \
                .x = {                                                  \
                        .start = {                                      \
                                .element_size = sizeof(metrics_vtable), \
                        },                                              \
                },                                                      \
        }

#define METRICS_FAMILY(_name, _description, _type, _get)                \
        {                                                               \
                .type = _METRICS_FAMILY,                                \
                .x = {                                                  \
                        .metric_family = {                              \
                                .name = _name,                          \
                                .description = _description,            \
                                .type = _type,                          \
                                .get = _get,                            \
                        },                                              \
                },                                                      \
        }

#define METRICS_VTABLE_END                                              \
        {                                                               \
                .type = _METRICS_VTABLE_END,                            \
                .x = {                                                  \
                        .end = {                                        \
                                ._reserved = 0,                         \
                        },                                              \
                },                                                      \
        }

#define JSON_BUILD_METRIC_NAME(name) SD_JSON_BUILD_PAIR_STRING("name", name)
#define JSON_BUILD_METRIC_VALUE(value) SD_JSON_BUILD_PAIR_UNSIGNED("value", value)
#define JSON_BUILD_METRIC_OBJECT(object) SD_JSON_BUILD_PAIR_STRING("object", object)
#define JSON_BUILD_METRIC_FIELD(k, v) JSON_BUILD_PAIR_STRING_NON_EMPTY(k, v)
#define JSON_BUILD_METRIC_FIELDS(fields) SD_JSON_BUILD_PAIR("field", SD_JSON_BUILD_OBJECT("fields", fields))
#define JSON_BUILD_METRIC_FIELD5(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5) \
                JSON_BUILD_PAIR_STRING_NON_EMPTY(k1, v1), \
                JSON_BUILD_PAIR_STRING_NON_EMPTY(k2, v2), \
                JSON_BUILD_PAIR_STRING_NON_EMPTY(k3, v3), \
                JSON_BUILD_PAIR_STRING_NON_EMPTY(k4, v4), \
                JSON_BUILD_PAIR_STRING_NON_EMPTY(k5, v5)
#define JSON_BUILD_METRIC(vl, name, count, pair) sd_json_buildo(&vl, JSON_BUILD_METRIC_NAME(name), JSON_BUILD_METRIC_VALUE(count), pair)
