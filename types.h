#ifndef TYPES_H
#define TYPES_H

#include <sys/stat.h>

// ==================== DISK INTERFACE ====================

/**
 * Disk interface structure for managing filesystem storage
 * Provides memory-mapped access to disk image file
 */
typedef struct DiskInterface {
   struct mount *mp;
   struct vnode *vp;
   uint64_t total_blocks;
} DiskInterface;

// =================== Cache Structures ===================

/**
 * Represents a single cache entry containing a disk block
 */
typedef struct cache_entry_t
{
	bool dirty_bit;              // True if block has been modified and needs writeback
	int pin_count;               // Reference count for preventing eviction
	uint64_t block_number;       // Disk block number this entry represents
	uint64_t inode_number;       // Inode that owns this block (for data blocks)
	void *page_data;             // Pointer to the actual cached block data
	//pthread_mutex_t lock;	     // Lock for multithreading
} cache_entry_t;

/**
 * Main cache structure managing all cached disk blocks
 */
typedef struct cache
{
} cache;

// File types
typedef enum {
    FILE_TYPE_REGULAR = S_IFREG,
    FILE_TYPE_DIRECTORY = S_IFDIR,
    FILE_TYPE_SYMLINK = S_IFLNK
} FileType;

#endif
