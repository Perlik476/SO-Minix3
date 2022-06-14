#include <assert.h>
#include <lib.h>
#include <minix/rs.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int notify(int fd, int type) {
    message m;
    m.m_lc_vfs_notify.fd = fd;
    m.m_lc_vfs_notify.event = type;
    endpoint_t vfs_ep;
    minix_rs_lookup("vfs", &vfs_ep);
    errno = 0;
    int ret = _syscall(vfs_ep, VFS_NOTIFY, &m);
    assert(ret == 0 || ret == -1);
    return errno;
}