/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Gabriel Bauer
 */

#ifndef _MYFS_H_
#define _MYFS_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/fcntl.h>
#include <sys/conf.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include "superblock.h"
#include "disk.h"
#include "print.h"
#include "lock.h"
#include "btr.h"
#include "alloc.h"
#include "string.h"
#include "directory.h"
#include "metadata-api.h"
#include "journal.h"

MALLOC_DECLARE(M_MYFS);

/* Filesystem name */
#define MYFS_NAME	"myfs"

struct myfs_node {
	Inode *inode;
	char *relative_path;
};

/*
 * In-memory mount data structure
 */
struct myfs_mount {
	struct mount	*mnt;			/* Back pointer to mount */
	struct vnode     *mnt_devvp;
	Superblock	mnt_sb;		/* Superblock copy */
	struct g_consumer *mnt_cp;		/* GEOM consumer */
	/* Add more fields as needed */
};

/* DiskInterface operations */
DiskInterface *get_disk(void);
void set_disk(DiskInterface *set_disk);

/* VFS operations */
extern struct vfsops myfs_vfsops;

/* VNODE operations */
extern struct vop_vector myfs_vnodeops;

/* Function declarations - make them static to match definitions */
/* Remove these if functions are defined in the same file */
static int myfs_vfs_mount(struct mount *mp);
static int myfs_vfs_unmount(struct mount *mp, int mntflags);
static int myfs_vfs_root(struct mount *mp, int flags, struct vnode **vpp);
static int myfs_vfs_statfs(struct mount *mp, struct statfs *sbp);

static int myfs_vn_open(struct vop_open_args *ap);
static int myfs_vn_close(struct vop_close_args *ap);
static int myfs_vn_read(struct vop_read_args *ap);
static int myfs_vn_write(struct vop_write_args *ap);
static int myfs_vn_getattr(struct vop_getattr_args *ap);
static int myfs_vn_lookup(struct vop_lookup_args *ap);
static int myfs_vn_readdir(struct vop_readdir_args *ap);
static int myfs_vn_strategy(struct vop_strategy_args *ap);

#endif /* _MYFS_H_ */
