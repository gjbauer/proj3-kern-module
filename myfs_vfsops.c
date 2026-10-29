/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * VFS Operations for myfs
 */

#include "myfs.h"
#include "config.h"
#include "superblock.h"
#include "journal.h"
#include "alloc.h"

#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/conf.h>

MALLOC_DEFINE(M_MYFS, "myfs", "MyFS filesystem");

static int
myfs_vfs_mount(struct mount *mp)
{
	printf("myfs: ENTRY from='%s' path='%s'\n",
	mp->mnt_stat.f_mntfromname,
	mp->mnt_stat.f_mntonname);

	printf("myfs: sizeof myfs_mount: %ld\n", sizeof(struct myfs_mount));
	
	struct myfs_mount *mntdata;
	struct nameidata nd;
	struct vnode *odevvp, *devvp, *rootvp;
	struct buf *bp;
	struct cdev *dev;
	struct g_consumer *cp;
	int ronly, error;

	void *optval = NULL;
	int opt_error = vfs_getopt(mp->mnt_optnew, "from", &optval, NULL);

	if (opt_error == 0 && optval != NULL) {
		vfs_mountedfrom(mp, (const char *)optval);
		printf("myfs: set f_mntfromname to '%s'\n", (const char *)optval);
	} else {
		printf("myfs: 'from' option not found (error=%d)\n", opt_error);
		return (EINVAL);
	}

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;

	printf("myfs: mount called, flags=0x%lx, from='%s', fstype='%s', path='%s'\n",
	mp->mnt_flag,
	mp->mnt_stat.f_mntfromname,
	mp->mnt_stat.f_fstypename,
	mp->mnt_stat.f_mntonname);

	/* 1. Reject updates up front. */
	if (mp->mnt_flag & MNT_UPDATE)
	{
		return (EOPNOTSUPP);
	}

	/* 2. Resolve the device path to a device vnode. */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE,
	       mp->mnt_stat.f_mntfromname);
	error = namei(&nd);
	if (error)
	{
		return (error);
	}
	odevvp = nd.ni_vp;

	devvp = mntfs_allocvp(mp, odevvp);
	dev = devvp->v_rdev;
	if (atomic_cmpset_acq_ptr((uintptr_t *)&dev->si_mountpt, 0, (uintptr_t)mp) == 0) {
		mntfs_freevp(devvp);
		vrele(odevvp);
		return (EBUSY);
	}

	g_topology_lock();
	error = g_vfs_open(devvp, &cp, "myfs", ronly ? 0 : 1);
	g_topology_unlock();
	if (error != 0) {
		atomic_store_rel_ptr((uintptr_t *)&dev->si_mountpt, 0);
		vrele(odevvp);
		return (error);
	}

	vrele(odevvp);
	
	VOP_UNLOCK(devvp);
	printf("myfs: devvp type = %d (VCHR=%d, VBLK=%d), v_rdev=%p\n",
	devvp->v_type, VCHR, VBLK, devvp->v_rdev);

	/* 3. Read and validate the superblock from the device. */
	error = bread(devvp, 0, BLOCK_SIZE, NOCRED, &bp);
	if (error) {
		printf("bread error: %d", error);
		g_topology_lock();
		g_vfs_close(cp);
		g_topology_unlock();
		atomic_store_rel_ptr((uintptr_t *)&dev->si_mountpt, 0);
		return (error);
	}
	
	block_type_t *bt = (block_type_t *)bp->b_data;
	if (*bt != BLOCK_TYPE_SUPER) {
		brelse(bp);
		g_topology_lock();
		g_vfs_close(cp);
		g_topology_unlock();
		atomic_store_rel_ptr((uintptr_t *)&dev->si_mountpt, 0);
		return (EIO);
	}
	Superblock *sb = (Superblock *)(bt + 1);

	/* 4. Allocate and fill mount data. */
	mntdata = malloc(sizeof(*mntdata), M_MYFS, M_WAITOK | M_ZERO);
	mntdata->mnt       = mp;
	mntdata->mnt_devvp = devvp;
	memcpy(&mntdata->mnt_sb, sb, sizeof(struct Superblock));
	mntdata->mnt_cp	= cp;

	brelse(bp);

	/* 5. Hand the device vnode to the HAL. */
	set_disk(disk_open(mp, devvp));
	get_disk()->total_blocks = sb->total_blocks;

	mp->mnt_stat.f_fsid.val[0] = (int32_t)sb->magic_number;
	mp->mnt_stat.f_fsid.val[1] = 0;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_stat.f_bsize  = BLOCK_SIZE;
	mp->mnt_stat.f_iosize = USABLE_BLOCK_SIZE;

	/* 6. Get the root vnode. */
	error = getnewvnode("myfs", mp, &myfs_vnodeops, &rootvp);
	if (error) {
		g_topology_lock();
		g_vfs_close(cp);
		g_topology_unlock();
		atomic_store_rel_ptr((uintptr_t *)&dev->si_mountpt, 0);
		return (error);
	}

	vn_lock(rootvp, LK_EXCLUSIVE | LK_RETRY);
	VN_LOCK_AREC(rootvp);

	Inode *inode = MALLOC(sizeof(struct Inode));

	inode_read(get_disk(), NULL, sb->root_inode, inode);

	rootvp->v_data = inode;
	rootvp->v_type = VDIR;

	error = insmntque(rootvp, mp);
	if (error) {
		vgone(rootvp);
		g_topology_lock();
		g_vfs_close(cp);
		g_topology_unlock();
		atomic_store_rel_ptr((uintptr_t *)&dev->si_mountpt, 0);
		return (error);
	}

	vn_set_state(rootvp, VSTATE_CONSTRUCTED);

	vhold(rootvp);

	VOP_UNLOCK(rootvp);

	mp->mnt_data = mntdata;
	mntdata->mnt_rootvp = rootvp;
	/* NOTE: do NOT vput() here — the mount holds this reference. */

	printf("myfs: mounted successfully\n");
	return (0);
}

static int
myfs_vfs_unmount(struct mount *mp, int mntflags)
{
	struct myfs_mount *mntdata = mp->mnt_data;
	int error, flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if (mntdata->mnt_rootvp) {
		vrele(mntdata->mnt_rootvp);
		vdrop(mntdata->mnt_rootvp);
		mntdata->mnt_rootvp = NULL;
	}

	error = vflush(mp, 0, flags, curthread);
	if (error)
		return (error);

	if (get_disk()) {
		disk_close(get_disk());
		set_disk(NULL);
	}
	
	vn_lock(mntdata->mnt_devvp, LK_EXCLUSIVE | LK_RETRY);

	if (mntdata->mnt_devvp) {
		atomic_store_rel_ptr((uintptr_t *)&mntdata->mnt_devvp->v_rdev->si_mountpt, 0);
		mntfs_freevp(mntdata->mnt_devvp);
	}
	
	g_topology_lock();
	g_vfs_close(mntdata->mnt_cp);
	g_topology_unlock();

	free(mntdata, M_MYFS);
	mp->mnt_data = NULL;
	
	printf("myfs: unmounted successfully\n");
	return (0);
}

static int
myfs_vfs_root(struct mount *mp, int flags, struct vnode **vpp)
{
	struct vnode *nvp;
	int error;

	error = VFS_VGET(mp, 0, flags, &nvp);
	if (error)
		return (error);
	*vpp = nvp;
	return (0);
}

static int
myfs_vfs_statfs(struct mount *mp, struct statfs *sbp)
{
	struct myfs_mount *mntdata = mp->mnt_data;

	sbp->f_bsize  = BLOCK_SIZE;
	sbp->f_iosize = USABLE_BLOCK_SIZE;
	sbp->f_blocks = mntdata->mnt_sb.total_blocks;
	sbp->f_bfree  = mntdata->mnt_sb.free_blocks;
	sbp->f_bavail = mntdata->mnt_sb.free_blocks;
	return (0);
}

static vfs_init_t myfs_init;
static int
myfs_init(struct vfsconf *vfsp)
{
	mtx_init(get_lock(), "global_nbtrfs_lock", NULL, MTX_DEF);
	printf("myfs: filesystem initialized\n");
	return (0);
}

static vfs_uninit_t myfs_uninit;
static int
myfs_uninit(struct vfsconf *vfsp)
{
	mtx_destroy(get_lock());
	printf("myfs: filesystem uninitialized\n");
	return (0);
}

static int
myfs_vget(struct mount *mp,
	ino_t ino,
	int flags,
	struct vnode **vpp)
{
	Inode *ip;
	struct vnode *vp;
	int error;

	MPASS( (flags & LK_EXCLUSIVE) != 0 );

	error = vfs_hash_get(mp, ino, flags, curthread, vpp, NULL, NULL);
	if (error != 0)
		return (error);
	if (*vpp != NULL) {
		if (!VN_IS_DOOMED(*vpp))
			return (0);
		vgone(*vpp);
		vput(*vpp);
	}

	/*
	 * We must promote to an exclusive lock for vnode creation.  This
	 * can happen if lookup is passed LOCKSHARED.
	 */
	if ((flags & LK_TYPE_MASK) == LK_SHARED) {
		flags &= ~LK_TYPE_MASK;
		flags |= LK_EXCLUSIVE;
	}

	/*
	 * We do not lock vnode creation as it is believed to be too
	 * expensive for such rare case as simultaneous creation of vnode
	 * for same ino by different processes. We just allow them to race
	 * and check later to decide who wins. Let the race begin!
	 */

	ip = MALLOC(sizeof(struct Inode));

	/* Allocate a new vnode/inode. */
	error = getnewvnode("myfs", mp, &myfs_vnodeops, &vp);
	if (error) {
		*vpp = NULL;
		FREE(ip);
		return (error);
	}
	/*
	 * FFS supports recursive locking.
	 */
	lockmgr(vp->v_vnlock, LK_EXCLUSIVE | LK_NOWITNESS, NULL);
	VN_LOCK_AREC(vp);
	vp->v_data = ip;
	vp->v_bufobj.bo_bsize = BLOCK_SIZE;

	error = insmntque(vp, mp);
	if (error != 0) {
		FREE(ip);
		*vpp = NULL;
		return (error);
	}
	vp->v_vflag &= ~VV_FORCEINSMQ;
	error = vfs_hash_insert(vp, ino, flags, curthread, vpp, NULL, NULL);
	if (error != 0)
		return (error);
	if (*vpp != NULL) {
		return (0);
	}

	/* Read the disk contents for the inode, copy into the inode. */
	error = inode_read(get_disk(), NULL, ino, ip);
	if (error != 0) {
		/*
		 * The inode does not contain anything useful, so it
		 * would be misleading to leave it on its hash chain.
		 * With mode still zero, it will be unlinked and
		 * returned to the free list by vput().
		 */
		vgone(vp);
		vput(vp);
		*vpp = NULL;
		return (error);
	}

	/*
	 * Finish inode initialization.
	 */
	if (vp->v_type != VFIFO) {
		/* NBtrtFS supports shared locking for all files except fifos. */
		VN_LOCK_ASHARE(vp);
	}

	vn_set_state(vp, VSTATE_CONSTRUCTED);
	*vpp = vp;
	return (0);
}

struct vfsops myfs_vfsops = {
	.vfs_mount	= myfs_vfs_mount,
	.vfs_unmount	= myfs_vfs_unmount,
	.vfs_root	= myfs_vfs_root,
	.vfs_statfs	= myfs_vfs_statfs,
	.vfs_init	= myfs_init,
	.vfs_uninit	= myfs_uninit,
	.vfs_vget	= myfs_vget,
};

VFS_SET(myfs_vfsops, myfs, 0);
