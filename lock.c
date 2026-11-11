#include "lock.h"
#include "myfs.h"

struct sx s_lock;

struct sx *get_lock(void)
{
	return &s_lock;
}


void lock(struct sx *sx)
{
	sx_xlock(sx);
}

void unlock(struct sx *sx)
{
	sx_xunlock(sx);
}
