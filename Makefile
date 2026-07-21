# FreeBSD kernel module Makefile
KMOD= myfs
SRCS= vnode_if.h myfs_vfsops.c myfs_vnops.c myfs_subr.c
SRCS+= opt_compat.h

.include <bsd.kmod.mk>
