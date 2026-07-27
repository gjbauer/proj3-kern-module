#ifndef CACHE_H
#define CACHE_H

#include "myfs.h"

#include "config.h"
#include "disk.h"
#include "types.h"

/**
 * Retrieve a block from cache, loading from disk if necessary
 */
void*
get_block(DiskInterface* disk, cache *cache, uint64_t inum, uint64_t pnum);

/**
 * Write data to a cached block, marking it dirty
 */
void
write_block(DiskInterface* disk, cache *cache, void *buf, int64_t inum, uint64_t pnum);

/**
 * Increases pin count on a block by 1
 */
void
increase_pin_count(DiskInterface* disk, cache *cache, uint64_t inum, uint64_t pnum);

/**
 * Decreases pin count on a block by 1
 */
void
decrease_pin_count(DiskInterface* disk, cache *cache, uint64_t inum, uint64_t pnum);

/**
 * Sync all dirty blocks for a specific inode to disk
 */
void cache_fsync(DiskInterface* disk, cache *cache, uint64_t inum);

/**
 * Sync all dirty blocks in the cache to disk
 */
void cache_sync(DiskInterface* disk, cache *cache);

/**
 * Allocate and initialize a new cache structure
 */
cache*
alloc_cache(void);

/**
 * Free all memory associated with a cache structure
 */
void
free_cache(cache *cache);

#endif
