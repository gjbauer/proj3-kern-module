#ifndef LOCK_H
#define LOCK_H

struct mtx *get_lock(void);

void lock(struct mtx *mtx);

void unlock(struct mtx *mtx);

#endif
