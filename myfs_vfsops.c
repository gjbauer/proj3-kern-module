/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * VFS Operations for myfs
 */

#include "myfs.h"

MALLOC_DEFINE(M_MYFS, "myfs", "MyFS filesystem");

/* Make VFS operations static since they're only used via the vfsops structure */
static int
myfs_vfs_mount(struct mount *mp)
{
	struct myfs_mount *mntdata;
	struct vnode *rootvp;
	int error;

	/* Validate mount point */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	/* Allocate mount data */
	mntdata = malloc(sizeof(*mntdata), M_MYFS, M_WAITOK | M_ZERO);
	mntdata->mnt = mp;

	/* Initialize superblock (read from disk in real implementation) */
	mntdata->mnt_sb.sb_magic = MYFS_MAGIC;
	mntdata->mnt_sb.sb_block_size = MYFS_BLOCK_SIZE;
	mntdata->mnt_sb.sb_root_ino = MYFS_ROOT_INO;

	mp->mnt_data = mntdata;
	mp->mnt_stat.f_fsid.val[0] = (int32_t)MYFS_MAGIC;
	mp->mnt_stat.f_fsid.val[1] = 0;
	mp->mnt_flag |= MNT_LOCAL;

	/* Get root vnode */
	error = VFS_VGET(mp, MYFS_ROOT_INO, LK_EXCLUSIVE, &rootvp);
	if (error) {
		free(mntdata, M_MYFS);
		return (error);
	}

	mntdata->mnt_rootvp = rootvp;
	rootvp->v_type = VDIR;

	vput(rootvp);

	MNT_ILOCK(mp);
	mp->mnt_stat.f_bsize = MYFS_BLOCK_SIZE;
	mp->mnt_stat.f_iosize = MYFS_BLOCK_SIZE;
	MNT_IUNLOCK(mp);

	printf("myfs: mounted successfully\n");
	return (0);
}

static int
myfs_vfs_unmount(struct mount *mp, int mntflags)
{
	struct myfs_mount *mntdata = mp->mnt_data;
	int error;
	int flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/* Flush any pending I/O */
	error = vflush(mp, 0, flags, curthread);
	if (error)
		return (error);

	/* Free mount data */
	free(mntdata, M_MYFS);
	mp->mnt_data = NULL;

	printf("myfs: unmounted successfully\n");
	return (0);
}

static int
myfs_vfs_root(struct mount *mp, int flags, struct vnode **vpp)
{
	struct myfs_mount *mntdata = mp->mnt_data;

	/* Fixed: vget only takes 2 arguments in modern FreeBSD */
	return (vget(mntdata->mnt_rootvp, flags | LK_RETRY));
}

static int
myfs_vfs_statfs(struct mount *mp, struct statfs *sbp)
{
	struct myfs_mount *mntdata = mp->mnt_data;

	sbp->f_bsize = MYFS_BLOCK_SIZE;
	sbp->f_iosize = MYFS_BLOCK_SIZE;
	sbp->f_blocks = mntdata->mnt_sb.sb_total_blocks;
	sbp->f_bfree = mntdata->mnt_sb.sb_free_blocks;
	sbp->f_bavail = mntdata->mnt_sb.sb_free_blocks;
	sbp->f_files = mntdata->mnt_sb.sb_inode_count;
	sbp->f_ffree = mntdata->mnt_sb.sb_free_blocks; /* Approximate */

	return (0);
}

static vfs_init_t myfs_init;
static int
myfs_init(struct vfsconf *vfsp)
{
	printf("myfs: filesystem initialized\n");
	return (0);
}

static vfs_uninit_t myfs_uninit;
static int
myfs_uninit(struct vfsconf *vfsp)
{
	printf("myfs: filesystem uninitialized\n");
	return (0);
}

/* VFS operations structure */
struct vfsops myfs_vfsops = {
	.vfs_mount =		myfs_vfs_mount,
	.vfs_unmount =		myfs_vfs_unmount,
	.vfs_root =		myfs_vfs_root,
	.vfs_statfs =		myfs_vfs_statfs,
	.vfs_init =		myfs_init,
	.vfs_uninit =		myfs_uninit,
};

/* Declare filesystem */
VFS_SET(myfs_vfsops, myfs, VFCF_LOOPBACK);
