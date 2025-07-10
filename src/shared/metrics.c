#include <assert.h>

#include "log.h"
#include "metrics.h"
#include "mkdir-label.h"
#include "varlink-io.systemd.Metrics.h"
#include "varlink-util.h"

int metrics_setup_varlink_server(sd_varlink_server **m, sd_event *event, sd_varlink_method_t vl_method_list_cb, void *userdata, const char *socket) {
        _cleanup_(sd_varlink_server_unrefp) sd_varlink_server *s = NULL;
        int r;

        assert(event);
        assert(userdata);
        assert(socket);

        r = varlink_server_new(&s, SD_VARLINK_SERVER_INHERIT_USERDATA, userdata);
        if (r < 0)
                return log_error_errno(r, "Failed to allocate varlink server object: %m");

        r = sd_varlink_server_add_interface(s, &vl_interface_io_systemd_Metrics);
        if (r < 0)
                return log_error_errno(r, "Failed to add Metrics interface to varlink server: %m");

        r = sd_varlink_server_bind_method(
                        s, "io.systemd.Metrics.List", vl_method_list_cb);
        if (r < 0)
                return r;

        (void) mkdir_label("/run/systemd/metrics", 0755);
        if (r < 0 && r != -EEXIST)
                return log_error_errno(r, "Failed to create directory '%s/': %m", "/run/systemd/metrics");
        r = sd_varlink_server_listen_address(s, socket, 0666);
        if (r < 0)
                return r;
        r = sd_varlink_server_attach_event(s, event, SD_EVENT_PRIORITY_NORMAL);
        if (r < 0)
                return r;

        *m = TAKE_PTR(s);
        return 0;
}
