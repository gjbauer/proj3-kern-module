# FreeBSD kernel module Makefile
KMOD= myfs
SRCS= myfs_vfsops.c myfs_vnops.c cache.c disk.c superblock.c lock.c btr.c bitmap.c directory.c inode.c journal.c metadata-api.c string.c time.c hash.c alloc.c
SRCS+= opt_compat.h vnode_if.h 

.include <bsd.kmod.mk>
