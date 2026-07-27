#include "lock.h"
#include "myfs.h"

struct mtx lock;

struct mtx *get_lock()
{
	return &lock;
}


void lock(struct mtx *mtx)
{
	mtx_lock(mtx);
}

void unlock(struct mtx *mtx)
{
	mtx_unlock(mtx);
}
