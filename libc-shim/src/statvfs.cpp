#include "no-fortify.h"
#include "statvfs.h"

using namespace shim;

int shim::statvfs(const char *path, struct vfs *buf) {
    struct ::statvfs tmp = {};
    int ret = ::statvfs(path, &tmp);
    buf->f_bsize = tmp.f_bsize;
    buf->f_frsize = tmp.f_frsize;
    buf->f_blocks = tmp.f_blocks;
    buf->f_bfree = tmp.f_bfree;
    buf->f_bavail = tmp.f_bavail;
    buf->f_files = tmp.f_files;
    buf->f_ffree = tmp.f_ffree;
    buf->f_favail = tmp.f_favail;
    buf->f_fsid = tmp.f_fsid;
    buf->f_flag = tmp.f_flag;
    buf->f_namemax = tmp.f_namemax;

    return ret;
}

static void copy_statvfs(const struct ::statvfs &tmp, struct vfs *buf) {
    buf->f_bsize = tmp.f_bsize;
    buf->f_frsize = tmp.f_frsize;
    buf->f_blocks = tmp.f_blocks;
    buf->f_bfree = tmp.f_bfree;
    buf->f_bavail = tmp.f_bavail;
    buf->f_files = tmp.f_files;
    buf->f_ffree = tmp.f_ffree;
    buf->f_favail = tmp.f_favail;
    buf->f_fsid = tmp.f_fsid;
    buf->f_flag = tmp.f_flag;
    buf->f_namemax = tmp.f_namemax;
}

int shim::fstatvfs(int fd, struct vfs *buf) {
    struct ::statvfs tmp = {};
    int ret = ::fstatvfs(fd, &tmp);
    if (ret == 0)
        copy_statvfs(tmp, buf);
    return ret;
}

int shim::fstatvfs64(int fd, struct vfs *buf) {
    return shim::fstatvfs(fd, buf);
}

void shim::add_statvfs_shimmed_symbols(std::vector<shimmed_symbol> &list) {
    list.insert(list.end(), {
        {"statvfs", statvfs},
        {"statvfs64", statvfs},
        {"fstatvfs", fstatvfs},
        {"fstatvfs64", fstatvfs64}
    });
}
