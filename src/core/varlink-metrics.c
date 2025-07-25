/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "json-util.h"
#include "manager.h"
#include "sd-json.h"
#include "sd-varlink.h"
#include "varlink-metrics.h"

int vl_method_list(sd_varlink *link, sd_json_variant *parameters, sd_varlink_method_flags_t flags, void *userdata) {
        Manager *m = ASSERT_PTR(userdata);
        int r;

        assert(link);

        r = sd_varlink_dispatch(link, parameters, /* dispatch_table= */ NULL, /* userdata= */ NULL);
        if (r != 0)
                return r;

        return sd_varlink_replybo(
                link,
                SD_JSON_BUILD_PAIR_STRING("name", "units"),
                SD_JSON_BUILD_PAIR_UNSIGNED("value", hashmap_size(m->units)),
                SD_JSON_BUILD_PAIR("fields",
                        SD_JSON_BUILD_OBJECT(
                                SD_JSON_BUILD_PAIR_STRING("state", "active"))));
}
