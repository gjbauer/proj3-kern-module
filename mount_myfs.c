#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	struct iovec iov[7]; /* 3 pairs = 6 elements, +1 safety */
	int iovlen = 0;
	const char *fstype = "myfs";
	const char *from, *fspath;

	if (argc != 3) {
		fprintf(stderr, "usage: mount_myfs <device> <mountpoint>\n");
		exit(1);
	}

	from   = argv[1];
	fspath = argv[2];

	/* fstype */
	iov[iovlen].iov_base = __DECONST(char *, "fstype");
	iov[iovlen].iov_len  = strlen("fstype") + 1;
	iovlen++;
	iov[iovlen].iov_base = __DECONST(char *, fstype);
	iov[iovlen].iov_len  = strlen(fstype) + 1;
	iovlen++;

	/* fspath */
	iov[iovlen].iov_base = __DECONST(char *, "fspath");
	iov[iovlen].iov_len  = strlen("fspath") + 1;
	iovlen++;
	iov[iovlen].iov_base = __DECONST(char *, fspath);
	iov[iovlen].iov_len  = strlen(fspath) + 1;
	iovlen++;

	/* from - CRITICAL: must be included for disk filesystems */
	iov[iovlen].iov_base = __DECONST(char *, "from");
	iov[iovlen].iov_len  = strlen("from") + 1;
	iovlen++;
	iov[iovlen].iov_base = __DECONST(char *, from);
	iov[iovlen].iov_len  = strlen(from) + 1;
	iovlen++;

	if (nmount(iov, iovlen, 0) < 0)
		err(1, "nmount");

	return 0;
}

