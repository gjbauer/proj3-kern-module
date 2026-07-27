/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Vnode Operations for myfs
 */

#include "myfs.h"

DiskInterface* disk;

DiskInterface *get_disk(void)
{
	return disk;
}

void set_disk(DiskInterface *set_disk)
{
	disk = set_disk;
}

static int
myfs_vn_open(struct vop_open_args *ap)
{
	//struct vnode *vp = ap->a_vp;

	VNASSERT(vp->v_type == VREG || vp->v_type == VDIR, vp,
	    ("myfs_open: non-regular file or directory"));

	return (0);
}

static int
myfs_vn_close(struct vop_close_args *ap)
{
	/* Nothing special needed for close */
	return (0);
}

static int
myfs_vn_read(struct vop_read_args *ap)
{
	struct vnode *vp = ap->a_vp;
	//struct uio *uio = ap->a_uio;

	if (vp->v_type == VDIR)
		return (EISDIR);

	if (vp->v_type != VREG)
		return (EINVAL);

	/* Read implementation would go here */
	return (EOPNOTSUPP);
}

static int
myfs_vn_write(struct vop_write_args *ap)
{
	struct vnode *vp = ap->a_vp;
	//struct uio *uio = ap->a_uio;

	if (vp->v_type == VDIR)
		return (EISDIR);

	if (vp->v_type != VREG)
		return (EINVAL);

	/* Write implementation would go here */
	return (EOPNOTSUPP);
}

static int
myfs_vn_getattr(struct vop_getattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;

	/* Return default attributes */
	VATTR_NULL(vap);
	vap->va_type = vp->v_type;
	vap->va_mode = 0755;
	vap->va_nlink = 1;
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_fileid = 1;
	vap->va_size = 0;
	vap->va_blocksize = MYFS_BLOCK_SIZE;
	vap->va_bytes = 0;

	return (0);
}

static int
myfs_vn_lookup(struct vop_lookup_args *ap)
{
	/* Directory lookup implementation would go here */
	return (ENOENT);
}

static int
myfs_vn_readdir(struct vop_readdir_args *ap)
{
	struct uio *uio = ap->a_uio;
	struct dirent dirent;
	int error;

	if (ap->a_vp->v_type != VDIR)
		return (ENOTDIR);

	/* Simple readdir - return '.' and '..' */
	bzero(&dirent, sizeof(dirent));

	/* Return "." */
	dirent.d_fileno = 1;
	dirent.d_type = DT_DIR;
	dirent.d_namlen = 1;
	strcpy(dirent.d_name, ".");
	dirent.d_reclen = GENERIC_DIRSIZ(&dirent);

	if (uio->uio_offset == 0) {
		error = uiomove(&dirent, dirent.d_reclen, uio);
		if (error)
			return (error);
		uio->uio_offset = dirent.d_reclen;
	}

	/* Return ".." */
	dirent.d_fileno = 1;
	dirent.d_type = DT_DIR;
	dirent.d_namlen = 2;
	strcpy(dirent.d_name, "..");
	dirent.d_reclen = GENERIC_DIRSIZ(&dirent);

	if (uio->uio_offset == dirent.d_reclen) {
		error = uiomove(&dirent, dirent.d_reclen, uio);
		if (error)
			return (error);
	}

	return (0);
}

static int
myfs_vn_strategy(struct vop_strategy_args *ap)
{
	struct buf *bp = ap->a_bp;

	/* Block I/O strategy would go here */
	bp->b_error = EOPNOTSUPP;
	bp->b_ioflags |= BIO_ERROR;
	bufdone(bp);

	return (EOPNOTSUPP);
}

/* Vnode operations vectors */
struct vop_vector myfs_vnodeops = {
	.vop_default =		NULL,
	.vop_open =		myfs_vn_open,
	.vop_close =		myfs_vn_close,
	.vop_read =		myfs_vn_read,
	.vop_write =		myfs_vn_write,
	.vop_getattr =		myfs_vn_getattr,
	.vop_lookup =		myfs_vn_lookup,
	.vop_readdir =		myfs_vn_readdir,
	.vop_strategy =		myfs_vn_strategy,
};
VFS_VOP_VECTOR_REGISTER(myfs_vnodeops);
