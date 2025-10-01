# FreeBSD kernel module Makefile
KMOD= myfs
SRCS= myfs.c
SRCS+= opt_compat.h

.include <bsd.kmod.mk>
