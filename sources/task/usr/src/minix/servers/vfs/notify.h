#ifndef __VFS_NOTIFY_H
#define __VFS_NOTIFY_H

#include "const.h"

EXTERN struct notify_list {
    int is_used;
    int event_type;
    struct fproc *fp;
    struct vnode *vnode;
} notify_list[NR_NOTIFY];

void notify_handle_open(struct vnode *);

void notify_handle_move(struct vnode *);

#endif


