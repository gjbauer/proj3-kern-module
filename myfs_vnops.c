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
	struct vnode *vp = ap->a_vp;

	printf("myfs_open: ENTER vnode=%p type=%d\n",
		vp, vp->v_type);
	
	VNASSERT(vp->v_type == VREG || vp->v_type == VDIR, vp,
	    ("myfs_open: non-regular file or directory"));

	printf("myfs_open: EXIT\n");

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

	printf("myfs_getattr: ENTER vp=%p thread=%p\n",
		vp, curthread);

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
	vap->va_blocksize = BLOCK_SIZE;
	vap->va_bytes = 0;

	printf("myfs_getattr: EXIT vp=%p\n", vp);

	return (0);
}

static int
myfs_vn_lookup(struct vop_lookup_args *ap)
{
	/* Directory lookup implementation would go here */
	printf("myfs: lookup call\n");
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	int error, lock_flags;

	*ap->a_vpp = NULL;

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * The generic namei code handles crossing out of
	 * the filesystem at its mounted root.
	 */
	if (cnp->cn_flags & ISDOTDOT) {
		struct nameidata nd;
		char *ppath, *retbuf = NULL, *freebuf = NULL;

		/* 1. Ensure you do NOT hold the vnode lock here */
		lock_flags = lockstatus(dvp->v_vnlock);
		VOP_UNLOCK(dvp);

		/* 2. Get the full path */
		error = vn_fullpath(dvp, &retbuf, &freebuf);

		if (error == 0) {
			/* Success: retbuf points to the start of the path string */
			printf("Vnode path: %s\n", retbuf);
		} else {
			printf("Failed to resolve path, error: %d\n", error);
		}

		/* 3. Get parent path */

		ppath = parent_path(retbuf, count_l(retbuf));

		/* 4. Resolve parent path vnode */

		/* 4a. Initialize nameidata */
		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1, UIO_USERSPACE, ppath);

		/* 4b. Perform lookup */
		error = namei(&nd);
		if (error != 0) {
			/* Handle error */
			return error;
		}

		/* 4c. Access the vnode */
		*ap->a_vpp = nd.ni_vp;

		/* 4d. Clean up pathname buffer */
		NDFREE_PNBUF(&nd);

		/* 4e. Free parent bath buffer */
		FREE(ppath);

		/* 5. Always free freebuf if it was allocated, NOT retbuf */
		if (freebuf != NULL) {
			free(freebuf, M_TEMP);
		}

		/* 6. Re-lock vnode */
		vn_lock(dvp, lock_flags | LK_RETRY);

		return 0;
	}
	else if (!strcmp(cnp->cn_nameptr, ".")) {
		vref(dvp);
		*ap->a_vpp = dvp;
		return 0;
	}

	return (ENOENT);
}

static int
myfs_vn_readdir(struct vop_readdir_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct dirent dirent;
	DirEntry *entries = NULL;
	char *retbuf = NULL, *freebuf = NULL;
	int error, lock_flags;

	printf("myfs: readdir call\n");
	
	*ap->a_eofflag = 0;

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

	/* Resolve non-dot, non-dotdot files */

	/* 1. Ensure you do NOT hold the vnode lock here */
	lock_flags = lockstatus(vp->v_vnlock);
	VOP_UNLOCK(vp);

	/* 2. Get the full path */
	error = vn_fullpath(vp, &retbuf, &freebuf);

	if (error == 0) {
		/* Success: retbuf points to the start of the path string */
		printf("Vnode path: %s\n", retbuf);
	} else {
		printf("Failed to resolve path, error: %d\n", error);
		return (error);
	}

	/* 3. Get path from root */
	char *from_root = from_fs_root(retbuf);

	/* 4. List the contents of the directory */

	int count = directory_list(get_disk(), NULL, from_root, &entries);

	printf("myfs_readdir: count=%d entries=%p\n", count, entries);

	struct vnode *nvp;
	Inode *ip;

	/* 5. Fill in the dirent with information gathered */
	for (int i=0; i<count; i++)
	{
		dirent.d_fileno = i + 2;
		error = VFS_VGET(get_disk()->mp, entries[i].inode_number, LK_EXCLUSIVE | LK_RETRY, &nvp);
		if (error) {
			FREE(entries);
			vn_lock(vp, lock_flags | LK_RETRY);
			return (error);
		}
		ip = nvp->v_data;
		switch (ip->mode & S_IFMT) {
		case S_IFDIR:  dirent.d_type = DT_DIR;  break;
		case S_IFREG:  dirent.d_type = DT_REG;  break;
		case S_IFLNK:  dirent.d_type = DT_LNK;  break;
		case S_IFCHR:  dirent.d_type = DT_CHR;  break;
		case S_IFBLK:  dirent.d_type = DT_BLK;  break;
		case S_IFIFO:  dirent.d_type = DT_FIFO; break;
		case S_IFSOCK: dirent.d_type = DT_SOCK; break;
		default:       dirent.d_type = DT_UNKNOWN;  break;
		}
		dirent.d_namlen = strlen(entries[i].name);
		strcpy(dirent.d_name, entries[i].name);
		dirent.d_reclen = GENERIC_DIRSIZ(&dirent);

		if (uio->uio_offset == dirent.d_reclen) {
			error = uiomove(&dirent, dirent.d_reclen, uio);
			if (error) {
				vput(nvp);
				FREE(entries);
				vn_lock(vp, lock_flags | LK_RETRY);
				return (error);
			}
		}
		vput(nvp);
	}


	/* 6. Always free freebuf if it was allocated, NOT retbuf
	 * Free from_root buffer as well
	 */
	if (freebuf != NULL) {
		free(freebuf, M_TEMP);
		FREE(from_root);
	}

	/* 6. Re-lock vnode */
	vn_lock(vp, lock_flags | LK_RETRY);

	/* 7. Wipe and free entries structure */
	if (entries)
	{
		arc4random_buf(entries, count*sizeof(struct DirEntry));
		FREE(entries);
	}

	*ap->a_eofflag = 1;

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

static int
myfs_reclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp = ap->a_vp;

	printf("myfs_reclaim: ENTER vnode=%p type=%d data=%p\n",
		vp, vp->v_type, vp->v_data);

	if (vp->v_data)
		FREE(vp->v_data);
	vp->v_data = NULL;

	printf("myfs_reclaim: EXIT\n");

	return (0);
}

static int
myfs_vn_access(struct vop_access_args *ap)
{
    printf("myfs_access: ENTER vp=%p mode=0x%x\n",
        ap->a_vp, ap->a_accmode);

    return (0);  /* Diagnostic only: bypasses permission checks. */
}

/* Vnode operations vectors */
struct vop_vector myfs_vnodeops = {
	.vop_default =		&default_vnodeops,
	.vop_open =		myfs_vn_open,
	.vop_close =		myfs_vn_close,
	.vop_read =		myfs_vn_read,
	.vop_write =		myfs_vn_write,
	.vop_getattr =		myfs_vn_getattr,
	.vop_lookup =		myfs_vn_lookup,
	.vop_readdir =		myfs_vn_readdir,
	.vop_strategy =		myfs_vn_strategy,
	.vop_reclaim =		myfs_reclaim,
	.vop_access =		myfs_vn_access,
};
VFS_VOP_VECTOR_REGISTER(myfs_vnodeops);
