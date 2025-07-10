#pragma once

#include "sd-varlink.h"

int metrics_setup_varlink_server(sd_varlink_server **m, sd_event *event, sd_varlink_method_t vl_method_list_cb, void *userdata, const char *socket);
