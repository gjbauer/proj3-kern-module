#ifndef LOCK_H
#define LOCK_H

struct sx *get_lock(void);

void lock(struct sx *sx);

void unlock(struct sx *sx);

#endif
