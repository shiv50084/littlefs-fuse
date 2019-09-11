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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

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
    puts(__func__);
    printf("Try to open: %s\n", path);
    HANDLE fd = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_DIRECTORY, NULL);

    if (fd == INVALID_HANDLE_VALUE) {
        puts("Cannot open file\n");
        return -errno;
    }

    cfg->context = fd;

    // get sector size
    if (!cfg->block_size) {
        long ssize = 512;
        //ioctl();
        DWORD _test_size = 0;

        GetFileSize(fd, &_test_size);
        printf("Size: %lu\n", _test_size);
        //int err = ioctl(fd, BLKSSZGET, &ssize);
        int err = 0;
        if (err) {
            return -errno;
        }
        cfg->block_size = ssize;
    }

    // get size in sectors
    if (!cfg->block_count) {
        long size = 64;
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
    puts(__func__);
    //HFILE fd = (HFILE)cfg->context;
    HANDLE fd = (HANDLE)cfg->context;
    CloseHandle(fd);
    //int fd = (int)cfg->context;
    //_close(fd);
}

int lfs_fuse_bd_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size) {
    puts(__func__);
    HANDLE fd = (HANDLE)cfg->context;

    // check if read is valid
    assert(block < cfg->block_count);

    // go to block
    lfs_off_t err = SetFilePointer(fd, (lfs_off_t)block * cfg->block_size + (lfs_off_t)off, NULL, SEEK_SET);
    if (err < 0) {
        puts("SEEK ERROR");
        return -errno;
    }

    // read block
    ssize_t read_bytes = 0;

    if (!ReadFile(fd, buffer, size, &read_bytes, NULL) || read_bytes < 0) {
        puts("ERROR READ");
        return -errno;
    }

    return 0;
}

int lfs_fuse_bd_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size) {
    puts(__func__);
    HANDLE fd = (HANDLE)cfg->context;

    // check if write is valid
    assert(block < cfg->block_count);

    // go to block
    lfs_off_t err = SetFilePointer(fd, (lfs_off_t)block * cfg->block_size + (lfs_off_t)off, NULL, SEEK_SET);
    if (err < 0) {
        return -errno;
    }

    // write block
    DWORD write_bytes = 0;

    if (!WriteFile(fd, buffer, size, &write_bytes, NULL)) {
        puts("ERROR WRITE");
        return -errno;
    }

    return 0;
}

int lfs_fuse_bd_erase(const struct lfs_config *cfg, lfs_block_t block) {
    // do nothing
    return 0;
}

int lfs_fuse_bd_sync(const struct lfs_config *cfg) {
    puts(__func__);
    HANDLE fd = (HANDLE)cfg->context;


    //int err = fsync(fd);
    if (!FlushFileBuffers(fd)) {
        puts("ERROR FLUSH");
        return -errno;
    }

    return 0;
}

