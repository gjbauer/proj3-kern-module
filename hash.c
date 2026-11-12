#include "hash.h"
#include "superblock.h"
#ifdef __linux__
#include <bsd/stdlib.h>
#endif
#ifndef _KERNEL
#include <stdlib.h>
#include <string.h>
#endif
#include "btr.h"
#include "journal.h"
#include "string.h"
#include "alloc.h"

/**
 * FNV-1a hash function implementation for filesystem path hashing
 * Provides good distribution and is fast for string hashing
 * Used for efficient path lookups in directory structures
 */
uint64_t path_hash(const char *path) {
    uint64_t hash = 0xcbf29ce484222325ULL; // FNV-1a offset basis (64-bit)
    
    // Process each character in the path string
    for (; *path; ++path) {
        hash ^= (uint64_t)(unsigned char)(*path);  // XOR with current byte
        hash *= 0x100000001b3ULL; // Multiply by FNV prime (64-bit)
    }
    
    return hash;
}

/**
 * This function takes a given absolute path and returns the corresponding
 * inode and, if a directory, new B-Tree root as found in the B-Tree search process.
 */
InodeBtreePair * item_search(DiskInterface* disk, cache *cache, const char *path)
{
	printf("item_search: before malloc\n");
    InodeBtreePair *pair = MALLOC(sizeof(struct InodeBtreePair));
	printf("item_search: after malloc\n");
    Superblock sb;
    BTreeNode *node = MALLOC(sizeof(struct BTreeNode));
    char curr_path[PATH_MAX];
	printf("item_search: before memset\n");
    memset(curr_path, '\0', PATH_MAX);
	printf("item_search: after memset\n");
    
    pair->inode_number = 0;
    pair->btree_block = 0;
	printf("item_search: before sb read\n");
    superblock_read(disk, cache, &sb);
	printf("item_search: after sb read\n");
	printf("item_search: before btree read\n");
    btree_node_read(disk, cache, sb.btree_root, node);
	printf("item_search: after btree read\n");
    
    if (!strcmp("/", path))
    {
        pair->inode_number = node->value;
        pair->btree_block = node->block_number;
        goto return_pair;
    }
    
    char *token;
    uint64_t node_block;
    
    for (int i=1; i <= count_l(path); i++) {
        token = split(path, i);
        printf("Searching for %s, hash = %lu\n", token, path_hash(token));
        //btree_print(disk, cache, node->block_number, 0);
        node_block = btree_search(disk, cache, node->block_number, path_hash(token));
        if (node_block)
        {
            size_t len = strlen(curr_path);
            snprintf(curr_path + len, sizeof(curr_path) - len, "/%s", token);
            btree_node_read(disk, cache, node_block, node);
            // If not a directory, this will not be a valid B-Tree node, and its contents will not copy...
            if (FILE_TYPE_DIRECTORY == node->type)
            {
                if (btree_node_read(disk, cache, node->value, node)) goto wipe_token;
            }
            if (!strcmp(path, curr_path))
            {
                if (FILE_TYPE_DIRECTORY == node->type) pair->btree_block = node->block_number;
                pair->inode_number = node->value;
                goto wipe_token;
            }
        }
        else goto wipe_token;
        arc4random_buf(token, strlen(token) * sizeof(char));
	FREE(token);
    }
    
    FPRINTF("ERROR: Path not found!!\n");
    goto return_pair;
wipe_token:
    printf("before arc4random\n");
    //arc4random_buf(token, strlen(token) * sizeof(char));
    printf("after arc4random\n");
    FREE(token);
return_pair:
    printf("before arc4random\n");
    //arc4random_buf(&sb, sizeof(struct Superblock));
    //arc4random_buf(node, sizeof(struct BTreeNode));
    FREE(node);
    //arc4random_buf(&curr_path, sizeof(curr_path));
    printf("after arc4random\n");
    return pair;
}

void
print_pair(InodeBtreePair *pair)
{
    printf("== PAIR PRINT ==\n");
    printf("inode: %lu\n", pair->inode_number);
    printf("btree: %lu\n", pair->btree_block);
    printf("== END PRINT ==\n");
}
