#pragma once

#include "sd-varlink.h"

int metrics_setup_varlink_server(
                sd_varlink_server **server, /* in and out param */
                sd_varlink_server_flags_t flags,
                sd_event *event,
                sd_varlink_method_t vl_method_list_cb,
                void *userdata);

int metrics_listen_varlink_address(sd_varlink_server *server, const char *address);
