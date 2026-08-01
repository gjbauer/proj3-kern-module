#include "lock.h"
#include "myfs.h"

struct mtx s_lock;

struct mtx *get_lock(void)
{
	return &s_lock;
}


void lock(struct mtx *mtx)
{
	mtx_lock(mtx);
}

void unlock(struct mtx *mtx)
{
	mtx_unlock(mtx);
}
