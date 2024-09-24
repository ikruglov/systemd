/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "cgroup-util.h"
#include "fd-util.h"
#include "iovec-util.h"
#include "machined.h"
#include "process-util.h"
#include "socket-util.h"
#include "strv.h"
#include "user-util.h"

int manager_get_machine_by_pid(Manager *m, pid_t pid, Machine **ret) {
        Machine *mm;
        int r;

        assert(m);
        assert(pid_is_valid(pid));
        assert(ret);

        mm = hashmap_get(m->machines_by_leader, &PIDREF_MAKE_FROM_PID(pid));
        if (!mm) {
                _cleanup_free_ char *unit = NULL;

                r = cg_pid_get_unit(pid, &unit);
                if (r >= 0)
                        mm = hashmap_get(m->machines_by_unit, unit);
        }
        if (!mm) {
                *ret = NULL;
                return 0;
        }

        *ret = mm;
        return 1;
}

int manager_add_machine(Manager *m, const char *name, Machine **ret) {
        Machine *machine;
        int r;

        assert(m);
        assert(name);

        machine = hashmap_get(m->machines, name);
        if (!machine) {
                r = machine_new(_MACHINE_CLASS_INVALID, name, &machine);
                if (r < 0)
                        return r;

                r = machine_link(m, machine);
                if (r < 0) {
                        machine_free(machine);
                        return r;
                }
        }

        if (ret)
                *ret = machine;

        return 0;
}

int manager_find_machine_for_uid(Manager *m, uid_t uid, Machine **ret_machine, uid_t *ret_internal_uid) {
        Machine *machine;
        int r;

        assert(m);
        assert(uid_is_valid(uid));

        /* Finds the machine for the specified host UID and returns it along with the UID translated into the
         * internal UID inside the machine */

        HASHMAP_FOREACH(machine, m->machines) {
                uid_t converted;

                r = machine_owns_uid(machine, uid, &converted);
                if (r < 0)
                        return r;
                if (r) {
                        if (ret_machine)
                                *ret_machine = machine;

                        if (ret_internal_uid)
                                *ret_internal_uid = converted;

                        return true;
                }
        }

        if (ret_machine)
                *ret_machine = NULL;
        if (ret_internal_uid)
                *ret_internal_uid = UID_INVALID;

        return false;
}

int manager_find_machine_for_gid(Manager *m, gid_t gid, Machine **ret_machine, gid_t *ret_internal_gid) {
        Machine *machine;
        int r;

        assert(m);
        assert(gid_is_valid(gid));

        HASHMAP_FOREACH(machine, m->machines) {
                gid_t converted;

                r = machine_owns_gid(machine, gid, &converted);
                if (r < 0)
                        return r;
                if (r) {
                        if (ret_machine)
                                *ret_machine = machine;

                        if (ret_internal_gid)
                                *ret_internal_gid = converted;

                        return true;
                }
        }

        if (ret_machine)
                *ret_machine = NULL;
        if (ret_internal_gid)
                *ret_internal_gid = GID_INVALID;

        return false;
}

void manager_gc(Manager *m, bool drop_not_started) {
        Machine *machine;

        assert(m);

        while ((machine = LIST_POP(gc_queue, m->machine_gc_queue))) {
                machine->in_gc_queue = false;

                /* First, if we are not closing yet, initiate stopping */
                if (machine_may_gc(machine, drop_not_started) &&
                    machine_get_state(machine) != MACHINE_CLOSING)
                        machine_stop(machine);

                /* Now, the stop probably made this referenced
                 * again, but if it didn't, then it's time to let it
                 * go entirely. */
                if (machine_may_gc(machine, drop_not_started)) {
                        machine_finalize(machine);
                        machine_free(machine);
                }
        }
}

static int on_deferred_gc(sd_event_source *s, void *userdata) {
        manager_gc(userdata, /* drop_not_started= */ true);
        return 0;
}

void manager_enqueue_gc(Manager *m) {
        int r;

        assert(m);

        if (m->deferred_gc_event_source) {
                r = sd_event_source_set_enabled(m->deferred_gc_event_source, SD_EVENT_ONESHOT);
                if (r < 0)
                        log_warning_errno(r, "Failed to enable GC event source, ignoring: %m");

                return;
        }

        r = sd_event_add_defer(m->event, &m->deferred_gc_event_source, on_deferred_gc, m);
        if (r < 0)
                return (void) log_warning_errno(r, "Failed to allocate GC event source, ignoring: %m");

        r = sd_event_source_set_priority(m->deferred_gc_event_source, SD_EVENT_PRIORITY_IDLE);
        if (r < 0)
                log_warning_errno(r, "Failed to tweak priority of event source, ignoring: %m");

        (void) sd_event_source_set_description(m->deferred_gc_event_source, "deferred-gc");
}

int machine_get_addresses(Machine* machine, struct local_address **ret_addresses, int *reterr_errno) {
        assert(machine);
        assert(ret_addresses);
        assert(reterr_errno);

        switch (machine->class) {

        case MACHINE_HOST: {
                _cleanup_free_ struct local_address *addresses = NULL;
                int n;

                n = local_addresses(NULL, 0, AF_UNSPEC, &addresses);
                if (n < 0)
                        return n;

                *ret_addresses = TAKE_PTR(addresses);
                return n;
        }

        case MACHINE_CONTAINER: {
                _cleanup_close_pair_ int pair[2] = EBADF_PAIR;
                _cleanup_close_ int netns_fd = -EBADF;
                pid_t child;
                int r;

                r = in_same_namespace(0, machine->leader.pid, NAMESPACE_NET);
                if (r < 0)
                        return r;
                if (r > 0) {
                        *reterr_errno = r;
                        return -ENONET;
                }

                r = pidref_namespace_open(&machine->leader,
                                          /* ret_pidns_fd = */ NULL,
                                          /* ret_mntns_fd = */ NULL,
                                          &netns_fd,
                                          /* ret_userns_fd = */ NULL,
                                          /* ret_root_fd = */ NULL);
                if (r < 0)
                        return r;

                if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) < 0)
                        return -errno;

                r = namespace_fork("(sd-addrns)", "(sd-addr)", NULL, 0, FORK_RESET_SIGNALS|FORK_DEATHSIG_SIGKILL,
                                   -1, -1, netns_fd, -1, -1, &child);
                if (r < 0) {
                        *reterr_errno = r;
                        return -ENOEXEC;
                }
                if (r == 0) {
                        _cleanup_free_ struct local_address *addresses = NULL;
                        struct local_address *a;
                        int i, n;

                        pair[0] = safe_close(pair[0]);

                        n = local_addresses(NULL, 0, AF_UNSPEC, &addresses);
                        if (n < 0)
                                _exit(EXIT_FAILURE);

                        for (a = addresses, i = 0; i < n; a++, i++) {
                                struct iovec iov[2] = {
                                        { .iov_base = &a->family, .iov_len = sizeof(a->family) },
                                        { .iov_base = &a->address, .iov_len = FAMILY_ADDRESS_SIZE(a->family) },
                                };

                                r = writev(pair[1], iov, 2);
                                if (r < 0)
                                        _exit(EXIT_FAILURE);
                        }

                        pair[1] = safe_close(pair[1]);

                        _exit(EXIT_SUCCESS);
                }

                pair[1] = safe_close(pair[1]);

                _cleanup_free_ struct local_address *list = NULL;
                size_t n_list = 0;

                for (;;) {
                        int family;
                        ssize_t n;
                        union in_addr_union in_addr;
                        struct iovec iov[2];
                        struct msghdr mh = {
                                .msg_iov = iov,
                                .msg_iovlen = 2,
                        };

                        iov[0] = IOVEC_MAKE(&family, sizeof(family));
                        iov[1] = IOVEC_MAKE(&in_addr, sizeof(in_addr));

                        n = recvmsg_safe(pair[0], &mh, 0);
                        if (n < 0)
                                return n;
                        if ((size_t) n < sizeof(family))
                                break;

                        if ((size_t) n != sizeof(family) + FAMILY_ADDRESS_SIZE(family))
                                return -EIO;

                        r = add_local_address(&list,
                                              &n_list,
                                              /* ifindex = */ 0,
                                              /* scope = */ '\0',
                                              family,
                                              &in_addr);
                        if (r < 0)
                                return r;
                }

                r = wait_for_terminate_and_check("(sd-addrns)", child, 0);
                if (r < 0) {
                        *reterr_errno = r;
                        return -ECHILD;
                }
                if (r != EXIT_SUCCESS)
                        return -ESHUTDOWN;

                *ret_addresses = TAKE_PTR(list);
                return (int) n_list;
        }

        default:
            return -ENOTSUP;
        }
}
