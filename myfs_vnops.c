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
	
	printf("myfs_lookup: name=%s\n", cnp->cn_nameptr);

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * The generic namei code handles crossing out of
	 * the filesystem at its mounted root.
	 */
	if (cnp->cn_flags & ISDOTDOT) {
		char *ppath, *retbuf = NULL, *freebuf = NULL;
		
		/* 1. Get the full path */
		error = vn_fullpath(dvp, &retbuf, &freebuf);

		if (error == 0) {
			/* Success: retbuf points to the start of the path string */
			printf("Vnode path: %s\n", retbuf);
		} else {
			printf("Failed to resolve path, error: %d\n", error);
		}

		if (count_l(retbuf) > count_l(get_disk()->mount)) {
			char *path;
			path = from_fs_root(retbuf);
			ppath = parent_path(path, count_l(path));

			InodeBtreePair *pair = item_search(get_disk(), NULL, ppath);

			error = VFS_VGET(get_disk()->mp, pair->inode_number, LK_EXCLUSIVE | LK_RETRY, ap->a_vpp);
	
			free(freebuf, M_TEMP);
			FREE(ppath);
			FREE(pair);

			return (error);
		}

		struct nameidata nd;

		/* 2. Ensure you do NOT hold the vnode lock here */
		lock_flags = lockstatus(dvp->v_vnlock);
		VOP_UNLOCK(dvp);


		/* 3. Get parent path */

		ppath = parent_path(retbuf, count_l(retbuf));

		/* 4. Resolve parent path vnode */

		/* 4a. Initialize nameidata */
		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1, UIO_SYSSPACE, ppath);

		/* 4b. Perform lookup */
		error = namei(&nd);
		if (error != 0) {
			vn_lock(dvp, lock_flags | LK_RETRY);
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

	BTreeNode tree_node;

	if (dvp->v_vflag & VV_ROOT) {
		printf("myfs_lookup: first component\n");
		Superblock sb;
		superblock_read(get_disk(), NULL, &sb);

		btree_node_read(get_disk(), NULL, sb.btree_root, &tree_node);
	} else {
		char *path = NULL, *retbuf = NULL, *freebuf = NULL;

		error = vn_fullpath(dvp, &retbuf, &freebuf);
		
		if (error == 0) {
			/* Success: retbuf points to the start of the path string */
			printf("Vnode path: %s\n", retbuf);
		} else {
			printf("Failed to resolve path, error: %d\n", error);
		}

		path = from_fs_root(retbuf);
		InodeBtreePair *pair = item_search(get_disk(), NULL, path);
		btree_node_read(get_disk(), NULL, pair->btree_block, &tree_node);

		free(freebuf, M_TEMP);
		FREE(path);
		FREE(pair);
	}

	uint64_t node_block;

	printf("myfs_lookup: dvp=%p root=%d name=%.*s "
		"start_block=%ju\n",
		dvp,
		!!(dvp->v_vflag & VV_ROOT),
		(int)cnp->cn_namelen,
		cnp->cn_nameptr,
		(uintmax_t)tree_node.block_number);
	
	node_block = btree_search(get_disk(), NULL, tree_node.block_number, path_hash(cnp->cn_nameptr));

	btree_node_read(get_disk(), NULL, node_block, &tree_node);

	if (FILE_TYPE_DIRECTORY == tree_node.type)
	{
		if (btree_node_read(get_disk(), NULL, tree_node.value, &tree_node)) return (ENOENT);
	}
	
	error = VFS_VGET(get_disk()->mp, tree_node.value, LK_EXCLUSIVE | LK_RETRY, ap->a_vpp);

	if (error)
		return (error);

	return 0;
}

/*
 * Returns:
 *   0       Entry emitted or skipped because it was already read
 *   EAGAIN  Not enough space for this entry
 *   other   uiomove() error
 */
static int
myfs_emit_dirent(struct uio *uio, struct dirent *de,
    off_t *position)
{
	off_t next = *position + de->d_reclen;
	int error;

	/*
	 * Skip entries before the offset requested by the caller.
	 */
	if (next <= uio->uio_offset) {
		*position = next;
		return (0);
	}

	/*
	 * Don't emit a partial dirent.
	 */
	if (uio->uio_resid < de->d_reclen)
		return (EAGAIN);

	error = uiomove(de, de->d_reclen, uio);
	if (error != 0)
		return (error);

	*position = next;
	return (0);
}

static int
myfs_vn_readdir(struct vop_readdir_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct dirent dirent;
	DirEntry *entries = NULL;
	char *retbuf = NULL, *freebuf = NULL, *from_root = NULL;
	int error = 0, lock_flags;
	off_t position = 0;
	bool eof = false;

	printf("myfs: readdir call\n");
	

	if (ap->a_vp->v_type != VDIR)
		return (ENOTDIR);

	/* "." */
	bzero(&dirent, sizeof(dirent));
	dirent.d_fileno = 1;
	dirent.d_type = DT_DIR;
	dirent.d_namlen = 1;
	strcpy(dirent.d_name, ".");
	dirent.d_reclen = GENERIC_DIRSIZ(&dirent);

	error = myfs_emit_dirent(uio, &dirent, &position);
	if (error != 0)
		goto out;

	/* ".." */
	bzero(&dirent, sizeof(dirent));
	dirent.d_fileno = 1;
	dirent.d_type = DT_DIR;
	dirent.d_namlen = 2;
	strcpy(dirent.d_name, "..");
	dirent.d_reclen = GENERIC_DIRSIZ(&dirent);

	error = myfs_emit_dirent(uio, &dirent, &position);
	if (error != 0)
		goto out;

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
	from_root = from_fs_root(retbuf);

	/* 4. List the contents of the directory */

	int count = directory_list(get_disk(), NULL, from_root, &entries);

	printf("myfs_readdir: count=%d entries=%p\n", count, entries);

	struct vnode *nvp;
	Inode *ip;

	/* 5. Fill in the dirent with information gathered */
	for (int i=0; i<count; i++)
	{
		bzero(&dirent, sizeof(dirent));
		dirent.d_fileno = entries[i].inode_number;
		error = VFS_VGET(get_disk()->mp, entries[i].inode_number, LK_EXCLUSIVE | LK_RETRY, &nvp);
		if (error) {
			vn_lock(vp, lock_flags | LK_RETRY);
			goto out;
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
		vput(nvp);
		dirent.d_namlen = strlen(entries[i].name);
		strcpy(dirent.d_name, entries[i].name);
		dirent.d_reclen = GENERIC_DIRSIZ(&dirent);

		error = myfs_emit_dirent(uio, &dirent, &position);
		if (error != 0)
			goto out;
	}

	eof = true;

out:
	/*
	 * Free buffers allocated during directory enumeration.
	 * This assumes the caller owns entries and from_root.
	 */
	if (entries != NULL)
		FREE(entries);

	if (from_root != NULL)
		FREE(from_root);

	if (freebuf != NULL)
		free(freebuf, M_TEMP);

	/*
	 * Only re-lock if this function actually unlocked vp.
	 * Track that using a separate boolean.
	 */
	if (!VOP_ISLOCKED(vp))
		vn_lock(vp, lock_flags | LK_RETRY);

	if (error == EAGAIN) {
		/*
		 * Buffer filled before reaching the end.
		 * A later call can resume using uio_offset.
		 */
		error = 0;
	} else if (error == 0) {
		eof = true;
	}

	if (ap->a_eofflag != NULL)
		*ap->a_eofflag = eof ? 1 : 0;

	return (error);
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
