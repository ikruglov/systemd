#include <assert.h>

#include "log.h"
#include "metrics.h"
#include "varlink-serialize.h"
#include "varlink-io.systemd.Metrics.h"
#include "varlink-util.h"

int metrics_setup_varlink_server(
                sd_varlink_server **server, /* in and out param */
                sd_varlink_server_flags_t flags,
                sd_event *event,
                sd_varlink_method_t vl_method_list_cb,
                void *userdata) {
        _cleanup_(sd_varlink_server_unrefp) sd_varlink_server *s = NULL;
        int r;

        assert(server);
        assert(event);

        if (*server)
                return 0;

        r = varlink_server_new(&s, flags, userdata);
        if (r < 0)
                return log_debug_errno(r, "Failed to allocate varlink metrics server object: %m");

        r = sd_varlink_server_add_interface(s, &vl_interface_io_systemd_Metrics);
        if (r < 0)
                return log_debug_errno(r, "Failed to add varlink metrics interface to varlink server: %m");

        r = sd_varlink_server_bind_method(s, "io.systemd.Metrics.List", vl_method_list_cb);
        if (r < 0)
                return log_debug_errno(r, "Failed to register varlink metrics methods: %m");

        r = sd_varlink_server_attach_event(s, event, SD_EVENT_PRIORITY_NORMAL);
        if (r < 0)
                return log_debug_errno(r, "Failed to attach varlink metrics connection to event loop: %m");

        *server = TAKE_PTR(s);
        return 0;
}

int metrics_listen_varlink_address(sd_varlink_server *server, const char *address) {
        int r;

        assert(server);
        assert(address);

        /* a new server will have empty list of addresses anyway */
        if (varlink_server_contains_socket(server, address))
                return 0;

        r = sd_varlink_server_listen_address(server, address, 0666 | SD_VARLINK_SERVER_MODE_MKDIR_0755);
        if (r < 0)
                return log_debug_errno(r, "Failed to bind to metrics varlink socket '%s': %m", address);

        return 0;
}
