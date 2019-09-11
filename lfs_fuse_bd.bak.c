/*
 * Linux user-space block device wrapper
 *
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "lfs_fuse_bd.h"

#include <errno.h>
//#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#ifdef _MSC_VER
#include <Windows.h>

typedef long long ssize_t;
#endif /* _MSC_VER */

#if !defined(__FreeBSD__) && !defined(_MSC_VER)
#include <sys/ioctl.h>
#include <linux/fs.h>
#elif defined(__FreeBSD__)
#define BLKSSZGET DIOCGSECTORSIZE
#define BLKGETSIZE DIOCGMEDIASIZE
#include <sys/disk.h>
#endif

static OFSTRUCT buffer;


// Block device wrapper for user-space block devices
int lfs_fuse_bd_create(struct lfs_config *cfg, const char *path) {
    printf("Try to open: %s\n", path);
    HFILE fd = OpenFile(path, &buffer, OF_READWRITE);

    if (fd == HFILE_ERROR) {
        puts("Cannot open file\n");
        return -errno;
    }

    cfg->context = fd;

    // get sector size
    if (!cfg->block_size) {
        long ssize = 512;
        //int err = ioctl(fd, BLKSSZGET, &ssize);
        int err = 0;
        if (err) {
            return -errno;
        }
        cfg->block_size = ssize;
    }

    // get size in sectors
    if (!cfg->block_count) {
        long size = 512;
        //int err = ioctl(fd, BLKGETSIZE, &size);
        int err = 0;
        if (err) {
            return -errno;
        }
        cfg->block_count = size;
    }

    // setup function pointers
    cfg->read  = lfs_fuse_bd_read;
    cfg->prog  = lfs_fuse_bd_prog;
    cfg->erase = lfs_fuse_bd_erase;
    cfg->sync  = lfs_fuse_bd_sync;

    return 0;
}

void lfs_fuse_bd_destroy(const struct lfs_config *cfg) {
    HFILE fd = (HFILE)cfg->context;
    CloseHandle(fd);
}

int lfs_fuse_bd_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size) {
    HFILE fd = (HFILE)cfg->context;

    // check if read is valid
    assert(block < cfg->block_count);

    // go to block
    lfs_off_t err = _llseek(fd, (lfs_off_t)block*cfg->block_size + (lfs_off_t)off, SEEK_SET);
    if (err < 0) {
        return -errno;
    }

    // read block
    //ssize_t res = read(fd, buffer, (size_t)size);
    ssize_t read_bytes = 0;

    if (!ReadFile(fd, buffer, size, &read_bytes, NULL) || read_bytes < 0) {
        return -errno;
    }

    return 0;
}

int lfs_fuse_bd_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size) {
    HFILE fd = (HFILE)cfg->context;

    // check if write is valid
    assert(block < cfg->block_count);

    // go to block
    lfs_off_t err = _llseek(fd, (lfs_off_t)block * cfg->block_size + (lfs_off_t)off, SEEK_SET);
    if (err < 0) {
        return -errno;
    }

    // write block
    //ssize_t res = write(fd, buffer, (size_t)size);
    DWORD write_bytes = 0;

    if (!WriteFile(fd, buffer, size, &write_bytes, NULL)) {
        return -errno;
    }

    return 0;
}

int lfs_fuse_bd_erase(const struct lfs_config *cfg, lfs_block_t block) {
    // do nothing
    return 0;
}

int lfs_fuse_bd_sync(const struct lfs_config *cfg) {
    HFILE fd = (HFILE)cfg->context;

    //int err = fsync(fd);
    if (!FlushFileBuffers(fd)) {
        return -errno;
    }

    return 0;
}

