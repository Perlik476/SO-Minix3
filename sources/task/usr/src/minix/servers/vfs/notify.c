#include "fs.h"

#include "notify.h"
#include "errno.h"
#include "file.h"
#include <fcntl.h>

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
	int event = job_m_in.m_lc_vfs_notify.event;
	int fd = job_m_in.m_lc_vfs_notify.fd;

	struct filp *f = fp->fp_filp[fd];
	if (f == NULL || f->filp_count == 0) {
		return (EBADF);
	}

	switch (event) {
		case NOTIFY_OPEN:
		case NOTIFY_TRIOPEN:
		case NOTIFY_CREATE:
		case NOTIFY_MOVE:
			break;
		default:
			return (EINVAL);
	}

	if (event == NOTIFY_CREATE || event == NOTIFY_MOVE) {
		mode_t mode = f->filp_mode;
		if (!S_ISDIR(mode)) {
			return (ENOTDIR);
		}
	}

	for (size_t i = 0; i < NR_NOTIFY; i++) {
		if (!notify_list[i].is_used) {
			notify_list[i].is_used = 1;
			notify_list[i].event_type = event;
			notify_list[i].vnode = fp->fp_filp[fd]->filp_vno;
			notify_list[i].fp = fp;
			suspend(FP_BLOCKED_ON_NOTIFY);
			return (SUSPEND);
		}
	}
	return (ENONOTIFY);
}
