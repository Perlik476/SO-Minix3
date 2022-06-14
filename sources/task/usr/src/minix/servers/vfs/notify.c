#include "fs.h"

#define NOTIFY_OPEN 1
#define NOTIFY_TRIOPEN 2
#define NOTIFY_CREATE 4
#define NOTIFY_MOVE 8

#include "notify.h"
#include "errno.h"
#include "file.h"

struct notify_list notify_list[NR_NOTIFY];

void notify_handle_open(struct vnode *vnode) {
	for (size_t i = 0; i < NR_NOTIFY; i++) {
		if (notify_list[i].is_used && vnode == notify_list[i].vnode) {
			if (notify_list[i].event_type == NOTIFY_OPEN) {
				notify_list[i].is_used = 0;
				replycode(notify_list[i].fp->fp_endpoint, (OK));
			}
		}
	}
}

int do_notify(void) {
	for (size_t i = 0; i < NR_NOTIFY; i++) {
		if (!notify_list[i].is_used) {
			notify_list[i].is_used = 1;
			notify_list[i].event_type = job_m_in.m_lc_vfs_notify.event;
			notify_list[i].vnode = fp->fp_filp[job_m_in.m_lc_vfs_notify.fd]->filp_vno;
			notify_list[i].fp = fp;
			suspend(FP_BLOCKED_ON_NOTIFY);
			return (SUSPEND);
		}
	}
	return (ENONOTIFY);
}
