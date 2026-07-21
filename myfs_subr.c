/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Subroutines for myfs
 */

#include "myfs.h"

/*
 * Allocate a new vnode
 */
int
myfs_alloc_vnode(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp;
	int error;

	error = getnewvnode("myfs", mp, &myfs_vnodeops, &vp);
	if (error)
		return (error);

	*vpp = vp;
	return (0);
}
