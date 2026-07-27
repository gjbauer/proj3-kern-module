# FreeBSD kernel module Makefile
KMOD= myfs
SRCS= myfs_vfsops.c myfs_vnops.c cache.c disk.c superblock.c lock.c
SRCS+= opt_compat.h vnode_if.h 

.include <bsd.kmod.mk>
