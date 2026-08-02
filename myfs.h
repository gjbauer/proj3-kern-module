/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Your Name
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

#include "disk.h"
#include "superblock.h"
#include "print.h"
#include "lock.h"
#include "btr.h"

MALLOC_DECLARE(M_MYFS);

/* Filesystem name */
#define MYFS_NAME	"myfs"

/* Block size */
#define MYFS_BLOCK_SIZE	4096

/*
 * On-disk inode structure
 */
struct myfs_inode {
	uint16_t	i_mode;		/* File mode */
	uint16_t	i_nlink;	/* Number of links */
	uint32_t	i_uid;		/* Owner UID */
	uint32_t	i_gid;		/* Owner GID */
	uint64_t	i_size;		/* File size in bytes */
	uint64_t	i_blocks;	/* Number of blocks */
	uint32_t	i_block[12];	/* Direct block pointers */
	uint32_t	i_indirect;	/* Single indirect block */
	uint32_t	i_double_indirect; /* Double indirect block */
	uint64_t	i_atime;	/* Access time */
	uint64_t	i_mtime;	/* Modification time */
	uint64_t	i_ctime;	/* Creation time */
	/* Add more fields as needed */
};

/*
 * In-memory mount data structure
 */
struct myfs_mount {
	struct mount	*mnt;			/* Back pointer to mount */
	struct vnode	*mnt_rootvp;		/* Root vnode */
	Superblock	mnt_sb;		/* Superblock copy */
	dev_t		mnt_dev;		/* Device mounted */
	struct g_consumer *mnt_cp;		/* GEOM consumer */
	/* Add more fields as needed */
};

/*
 * In-memory inode data (vnode private data)
 */
struct myfs_node {
	struct vnode	*vp;			/* Back pointer to vnode */
	ino_t		n_ino;			/* Inode number */
	struct myfs_inode n_inode;		/* On-disk inode copy */
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
