#include "myfs.h"

#include "disk.h"
#include "config.h"
#include "cache.h"
#include "superblock.h"
#include "journal.h"
#include "lock.h"

/**
 * Open and memory-map a disk image file for filesystem operations
 * Creates a DiskInterface structure for accessing the disk
 */
DiskInterface* disk_open(struct mount *mp)
{
	DiskInterface *disk = (DiskInterface*)malloc(sizeof(struct DiskInterface), M_MYFS, M_WAITOK);
	disk->mp = mp;
	disk->vp = mp->mnt_vnodecovered;
	
	return disk;
}

/**
 * Close the disk interface and clean up resources
 * Unmaps memory and closes file handle
 */
void disk_close(DiskInterface* disk)
{
	free(disk, M_MYFS);
}

/**
 * Get a pointer to a specific block in the memory-mapped disk
 * Provides direct access to block data without copying
 */
void*
disk_get_block(DiskInterface* disk, int pnum)
{
	struct buf *bp;
	bread(disk->vp, 0, BLOCK_SIZE, NOCRED, &bp);
	return bp->b_data;
}

/**
 * Allocate a free block from the filesystem
 * Searches the block bitmap for the first available block
 */
uint64_t
alloc_page(DiskInterface* disk, cache *cache)
{
	int pbmn = 1;
    Superblock sb;
	void* pbm = get_block(disk, cache, 0, pbmn);
    superblock_read(disk, cache, &sb);

	// Search through all blocks to find first free one
	for (int ii = 0; ii < disk->total_blocks; ++ii) {
		if ( !(ii % USABLE_BLOCK_SIZE) && ii )
		{
			pbmn++;
			decrease_pin_count(disk, cache, 0, pbmn);
			pbm = get_block(disk, cache, 0, pbmn);
		}
		if (!bitmap_get(pbm, ii - ((pbmn - 1) * USABLE_BLOCK_SIZE))) {  // Found a free block
			if (bitmap_put(pbm, ii - ((pbmn - 1) * USABLE_BLOCK_SIZE), 1))  // Mark it as allocated
			{
				FPRINTF("ERROR: Could not allocate page!!\n");
				return 0;
			}
            
            sb.free_blocks--;
            
            superblock_write(disk, cache, &sb, false);
            
			write_block(disk, cache, pbm, 0, pbmn );
			lock(get_lock());
			disk_write_block(disk, pbmn, pbm);
			unlock(get_lock());
            
			//printf("+ alloc_page() -> %d\n", ii);
			return ii;
		}
	}

	FPRINTF("ERROR: No free blocks available for allocation!\n");
	return 0;  // No free blocks available
}

/**
 * Free a previously allocated block
 * Marks the block as available in the block bitmap
 */
void
free_page(DiskInterface* disk, cache *cache, int pnum)
{
	int pbmn = 1 + (pnum / USABLE_BLOCK_SIZE);
    Superblock sb;
	void* pbm = get_block(disk, cache, 0, pbmn );
    superblock_read(disk, cache, &sb);
    
	if (bitmap_put(pbm, pnum - ((pbmn - 1) * USABLE_BLOCK_SIZE), 0))  // Mark block as free
	{
		FPRINTF("ERROR: Selected block could not be freed!\n");
	}
	//printf("+ free_page(%d)\n", pnum);
    
    sb.free_blocks--;
    
    superblock_write(disk, cache, &sb, false);
    
    write_block(disk, cache, pbm, 0, pbmn );
    lock(get_lock());
	disk_write_block(disk, pbmn, pbm);
	unlock(get_lock());
}

/**
 * Read a block from disk into a buffer
 * Uses memory-mapped access for efficient copying
 */
int disk_read_block(DiskInterface* disk, uint64_t block_num, void* buffer)
{
	int rv = -1;
	void *block = disk_get_block(disk, block_num);
	
	// Copy block data to user buffer
	if (memcpy(buffer, block, BLOCK_SIZE)) {
		rv = 0;
	}
	

	struct buf *bp = incore(&disk->vp->v_bufobj, block_num);
	brelse(bp);
	return rv;
}

/**
 * Write buffer data to a block on disk
 * Uses memory-mapped access for efficient copying
 */
int disk_write_block(DiskInterface* disk, uint64_t block_num, const void* buffer)
{
	int rv = -1;
	void *block = disk_get_block(disk, block_num);
	
	// Copy user buffer to block location
	if (memcpy(block, buffer, BLOCK_SIZE)) {
		rv = 0;
	}
	
	return rv;
}

/**
 * Format the disk with a new filesystem
 * TODO: Implement filesystem formatting functionality
 */
int disk_format(DiskInterface* disk, cache *cache, const char* volume_name)
{
    Superblock superblock;

    if (superblock_initialize(disk, cache, volume_name)) FPRINTF("ERROR: Volume name too long\n");
    if (superblock_read(disk, cache, &superblock)) FPRINTF("ERROR: Invalid superblock!\n");
    printf("Size of journal entry = %lu\n", sizeof(struct journal_entry_t));
    printf("Setting block types to bitmaps for bitmaps...\n");
    block_type_t *block_type;
    for (int i=1; i < superblock.inode_bitmap+calculate_inode_bitmap_size(&superblock); i++ )
    {
        block_type = (block_type_t*)get_block(disk, cache, 0, i);
        *block_type = BLOCK_TYPE_BITMAP;
    }
    printf("Setting block types to INODE for inode table blocks...\n");
    for (int i=superblock.inode_bitmap+calculate_inode_bitmap_size(&superblock); i < ( superblock.inode_bitmap+calculate_inode_bitmap_size(&superblock)+calculate_inode_table_size(&superblock) ); i++ )
    {
        block_type = (block_type_t*)get_block(disk, cache, 0, i);
        *block_type = BLOCK_TYPE_INODE;
    }
    printf("Setting block types to JOURNAL for journal blocks and setting journal entry type to UNINITIALIZED...\n");
    for (int i=( superblock.inode_bitmap+calculate_inode_bitmap_size(&superblock)+calculate_inode_table_size(&superblock) ); i < ( superblock.inode_bitmap+calculate_inode_bitmap_size(&superblock)+calculate_inode_table_size(&superblock)+calculate_journal_size(&superblock) ); i++ )
    {
        block_type = (block_type_t*)get_block(disk, cache, 0, i);
        *block_type = BLOCK_TYPE_JOURNAL;
	journal_entry_t *entry = (journal_entry_t*)(block_type + 1);
	entry->type = TT_UNINITIALIZED;
    }
    printf("Allocating pages for superblock, bitmaps, and inode table...\n");
    alloc_page(disk, cache);
    for (int i=1; i < (superblock.inode_bitmap+calculate_inode_bitmap_size(&superblock)+calculate_inode_table_size(&superblock)) ; i++)
    {
        if(!alloc_page(disk, cache)) return -1;
    }
    //superblock.free_blocks = superblock.total_blocks - (superblock.inode_bitmap+calculate_inode_bitmap_size(&superblock)+calculate_inode_table_size(&superblock));
    printf("Usable block size / inode size : %lu\n", USABLE_BLOCK_SIZE/sizeof(struct Inode));

	printf("Allocating journal...\n");
	uint64_t journal_start = alloc_page(disk, cache);
	for(int i=1; i < calculate_journal_size(&superblock); i++)
	{
		if(!alloc_page(disk, cache)) return -1;
	}
	superblock.journal_start = journal_start;
	superblock.journal_head = 0;

    printf("Creating root tree node...\n");
    uint64_t page;
    BTreeNode *root = btree_node_create(disk, cache, false, &page);
    root->value = inode_allocate(disk, cache, S_IFDIR | 0755, true);
	superblock.root_inode = root->value;
    superblock.btree_root = page;
    
    printf("Free blocks: %lu\n", superblock.free_blocks);

    printf("Writing superblock...\n");
    superblock_write(disk, cache, &superblock, true);
    
    return 0;
}
