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
	struct uio *uio = ap->a_uio;
	struct myfs_node *fs_node = vp->v_data;
	Inode *node = fs_node->inode;
	int rv = 0;

	if (vp->v_type == VDIR)
		return (EISDIR);

	if (vp->v_type != VREG)
		return (EINVAL);

	// Check if offset is beyond file size
	if (uio->uio_offset >= node->size) {
		goto exit; // Nothing to read
	}

	// Adjust size if it would read beyond the end of the file
	/*if (uio->uio_offset + uio->uio_resid > node->size) {
		uio->uio_resid = node->size - uio->uio_offset;
	}*/

	if (uio->uio_resid == 0) {
		goto exit;
	}

	int remaining = uio->uio_resid;
	off_t current_offset = uio->uio_offset;

	while (remaining > 0) {
		// Calculate which page we're reading from
		int page_index = current_offset / USABLE_BLOCK_SIZE;
		int page_offset = current_offset % USABLE_BLOCK_SIZE;

		// Get the physical page number for this logical page
		uint64_t pnum = 0;

		rv = inode_get_block(get_disk(), NULL, node, page_index, &pnum);

		if (rv) {
			goto exit;
		}

		if (!pnum) {
			break; // No page allocated here
		}

		// Get pointer to the page data
		block_type_t *block_type = (block_type_t*) get_block(get_disk(), NULL, node->inode_number, pnum);
		// For data blocks, we need to skip the block type header
		char *data_area = (char*)(block_type + 1);

		// Adjust calculations to account for block type header
		int data_offset = page_offset;
		int available_data = USABLE_BLOCK_SIZE - data_offset;
		int bytes_to_read = available_data;
		if (bytes_to_read > remaining) {
			bytes_to_read = remaining;
		}

		// Copy data from page to buffer (after block type header)
		vn_io_fault_uiomove(data_area, bytes_to_read, uio);

		// Update counters
		remaining -= bytes_to_read;
		current_offset += bytes_to_read;
	}
	rv = 0;

exit:
	printf("read(%s, %ld bytes, @+%ld) -> %d\n", fs_node->relative_path, uio->uio_resid, uio->uio_offset, rv);
	return rv;
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
myfs_vn_create(struct vop_create_args *ap)
{
	printf("myfs create call\n");
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp = ap->a_vpp;
	struct myfs_node *fs_node = (*vpp)->v_data;
	struct componentname *cnp = ap->a_cnp;
	int rv = -1;
	char *relative_path = NULL;
	if (strlen(fs_node->relative_path) == 1)
	{
		relative_path = MALLOC(cnp->cn_namelen + 2);
		snprintf(relative_path, cnp->cn_namelen + 2, "/%s", cnp->cn_nameptr);
	}
	else
	{
		relative_path = MALLOC(cnp->cn_namelen + strlen(fs_node->relative_path) + 2);
		snprintf(relative_path, cnp->cn_namelen + strlen(fs_node->relative_path) + 2, "%s/%s", fs_node->relative_path, cnp->cn_nameptr);
	}
	journal_entry_t entry;
	entry.type = TT_MKNOD;
	entry.synced = false;
	strncpy(entry.mknod.path, relative_path, PATH_MAX);
	switch (vap->va_type) {
		case VDIR: entry.mknod.mode = S_IFDIR | vap->va_mode; break;
		case VREG: entry.mknod.mode = S_IFREG | vap->va_mode; break;
		case VLNK: entry.mknod.mode = S_IFLNK | vap->va_mode; break;
		case VCHR: entry.mknod.mode = S_IFCHR | vap->va_mode; break;
		case VBLK: entry.mknod.mode = S_IFBLK | vap->va_mode; break;
		case VFIFO: entry.mknod.mode = S_IFIFO | vap->va_mode; break;
		case VSOCK: entry.mknod.mode = S_IFSOCK | vap->va_mode; break;
		default: entry.mknod.mode = 0 | vap->va_mode; break;
	}

	entry.mknod.inode_number = 0;
	rv = _mknod(get_disk(), NULL, relative_path, entry.mknod.mode, 0, false, &entry.mknod.inode_number);
	initialize_journal_entry(get_disk(), NULL, &entry);
	FREE(relative_path);
	return rv;
}
static int
myfs_vn_mknod(struct vop_mknod_args *ap)
{
	printf("myfs mknod call\n");
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp = ap->a_vpp;
	struct myfs_node *fs_node = (*vpp)->v_data;
	struct componentname *cnp = ap->a_cnp;
	int rv = -1;
	char *relative_path = NULL;
	if (strlen(fs_node->relative_path) == 1)
	{
		relative_path = MALLOC(cnp->cn_namelen + 2);
		snprintf(relative_path, cnp->cn_namelen + 2, "/%s", cnp->cn_nameptr);
	}
	else
	{
		relative_path = MALLOC(cnp->cn_namelen + strlen(fs_node->relative_path) + 2);
		snprintf(relative_path, cnp->cn_namelen + strlen(fs_node->relative_path) + 2, "%s/%s", fs_node->relative_path, cnp->cn_nameptr);
	}
	journal_entry_t entry;
	entry.type = TT_MKNOD;
	entry.synced = false;
	strncpy(entry.mknod.path, relative_path, PATH_MAX);
	switch (vap->va_type) {
		case VDIR: entry.mknod.mode = S_IFDIR | vap->va_mode; break;
		case VREG: entry.mknod.mode = S_IFREG | vap->va_mode; break;
		case VLNK: entry.mknod.mode = S_IFLNK | vap->va_mode; break;
		case VCHR: entry.mknod.mode = S_IFCHR | vap->va_mode; break;
		case VBLK: entry.mknod.mode = S_IFBLK | vap->va_mode; break;
		case VFIFO: entry.mknod.mode = S_IFIFO | vap->va_mode; break;
		case VSOCK: entry.mknod.mode = S_IFSOCK | vap->va_mode; break;
		default: entry.mknod.mode = 0 | vap->va_mode; break;
	}

	entry.mknod.inode_number = 0;
	rv = _mknod(get_disk(), NULL, relative_path, entry.mknod.mode, 0, false, &entry.mknod.inode_number);
	initialize_journal_entry(get_disk(), NULL, &entry);
	FREE(relative_path);

	vgone(*vpp);
	vput(*vpp);
	rv = VFS_VGET(ap->a_dvp->v_mount, entry.mknod.inode_number, LK_EXCLUSIVE | LK_RETRY, vpp);
	if (rv) {
		*vpp = NULL;
		return (rv);
	}

	return rv;
}

static int
myfs_vn_getattr(struct vop_getattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	struct myfs_node *mfs_node = vp->v_data;
	Inode *ip = mfs_node->inode;

	printf("myfs_getattr: ENTER vp=%p thread=%p\n",
		vp, curthread);

	/* Return default attributes */
	VATTR_NULL(vap);
	vap->va_type = vp->v_type;
	vap->va_mode = ip->mode & 07777;
	vap->va_nlink = 1;
	vap->va_uid = ip->owner_id;
	vap->va_gid = ip->group_id;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_fileid = ip->inode_number;
	vap->va_size = ip->size;
	vap->va_blocksize = BLOCK_SIZE;
	vap->va_bytes = ip->size;

	printf("myfs_getattr: EXIT vp=%p\n", vp);

	return (0);
}

static int
myfs_vn_lookup(struct vop_lookup_args *ap)
{
	/* Directory lookup implementation would go here */
	printf("myfs: lookup call\n");
	struct vnode *dvp = ap->a_dvp;
	struct myfs_node *dnode = dvp->v_data;
	struct componentname *cnp = ap->a_cnp;
	int error, lock_flags;

	*ap->a_vpp = NULL;

	printf("myfs_lookup: name='%.*s' len=%d "
		"flags=0x%lx ISDOTDOT=%d ISLASTCN=%d "
		"dvp=%p locked=%d\n",
		(int)cnp->cn_namelen,
		cnp->cn_nameptr,
		(int)cnp->cn_namelen,
		cnp->cn_flags,
		!!(cnp->cn_flags & ISDOTDOT),
		!!(cnp->cn_flags & ISLASTCN),
		dvp,
		VOP_ISLOCKED(dvp));
	
	printf("myfs_lookup: name=%s\n", cnp->cn_nameptr);

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * The generic namei code handles crossing out of
	 * the filesystem at its mounted root.
	 */
	if (cnp->cn_flags & ISDOTDOT) {
		char *ppath;
		if (strcmp(dnode->relative_path, "/")) {
			ppath = parent_path(dnode->relative_path, count_l(dnode->relative_path));

			InodeBtreePair *pair = item_search(get_disk(), NULL, ppath);

			error = VFS_VGET(get_disk()->mp, pair->inode_number, LK_EXCLUSIVE | LK_RETRY, ap->a_vpp);
	
			FREE(ppath);
			FREE(pair);

			return (error);
		}
		char *retbuf = NULL, *freebuf = NULL;

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

		struct nameidata nd;

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
	
	char *relative_path = NULL;
	if (strlen(dnode->relative_path) == 1)
	{
		relative_path = MALLOC(cnp->cn_namelen + 2);
		snprintf(relative_path, cnp->cn_namelen + 2, "/%s", cnp->cn_nameptr);
	}
	else
	{
		relative_path = MALLOC(cnp->cn_namelen + strlen(dnode->relative_path) + 2);
		snprintf(relative_path, cnp->cn_namelen + strlen(dnode->relative_path) + 2, "%s/%s", dnode->relative_path, cnp->cn_nameptr);
	}

	InodeBtreePair *pair = item_search(get_disk(), NULL, relative_path);
	error = VFS_VGET(get_disk()->mp, pair->inode_number, LK_EXCLUSIVE | LK_RETRY, ap->a_vpp);
	FREE(pair);

	struct myfs_node *nnode = (*ap->a_vpp)->v_data;

	nnode->relative_path = relative_path;

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
	char *from_root = NULL;
	int error = 0;
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

	/* 1. Extract relative path */
	struct myfs_node *node = vp->v_data;

	/* 2. List contents of directory */
	int count = directory_list(get_disk(), NULL, node->relative_path, &entries);

	printf("myfs_readdir: count=%d entries=%p\n", count, entries);

	struct vnode *nvp;
	Inode *ip;

	/* 3. Fill in the dirent with information gathered */
	for (int i=0; i<count; i++)
	{
		bzero(&dirent, sizeof(dirent));
		dirent.d_fileno = entries[i].inode_number;
		error = VFS_VGET(get_disk()->mp, entries[i].inode_number, LK_EXCLUSIVE | LK_RETRY, &nvp);
		if (error)
			goto out;
		node = nvp->v_data;
		ip = node->inode;
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
	struct myfs_node *node = vp->v_data;

	printf("myfs_reclaim: ENTER vnode=%p type=%d data=%p\n",
		vp, vp->v_type, vp->v_data);

	if (vp->v_data) {
		if (node->inode)
			FREE(node->inode);
		if (node->relative_path)
			FREE(node->relative_path);
		FREE(vp->v_data);
	}
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
	.vop_mknod =		myfs_vn_mknod,
	.vop_create =		myfs_vn_create,
};
VFS_VOP_VECTOR_REGISTER(myfs_vnodeops);
