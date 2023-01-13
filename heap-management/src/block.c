/* block.c: Block Structure */

#include "malloc/block.h"
#include "malloc/counters.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/**
 * Allocate a new block on the heap using sbrk:
 *
 *  1. Determined aligned amount of memory to allocate.
 *  2. Allocate memory on the heap.
 *  3. Set allocage block properties.
 *
 * @param   size    Number of bytes to allocate.
 * @return  Pointer to data portion of newly allocate block.
 **/
Block*	block_allocate(size_t size) {
    // Allocate block
    intptr_t allocated = sizeof(Block) + ALIGN(size);
    Block*  block     = sbrk(allocated);
    if (block == SBRK_FAILURE) {
    	return NULL;
    }
    // Record block information
    block->capacity = ALIGN(size);
    block->size     = size;
    block->prev     = block;
    block->next     = block;
    // Update counters
    Counters[HEAP_SIZE] += allocated;
    Counters[BLOCKS]++;
    Counters[GROWS]++;
    return block;
}

/**
 * Attempt to release memory used by block to heap:
 *
 *  1. If the block is at the end of the heap.
 *  2. The block capacity meets the trim threshold.
 *
 * @param   block   Pointer to block to release.
 * @return  Whether or not the release completed successfully.
 **/
bool	block_release(Block *block) {
    Block* heap_end = sbrk(0);

    if(!heap_end) {
        return false;
    }

    if((heap_end == (block + 1 + block->capacity/sizeof(Block))) && block->capacity >= TRIM_THRESHOLD) {
        Counters[BLOCKS]--;
        Counters[SHRINKS]++;
        size_t allocated = sizeof(Block)+block->capacity;
        Counters[HEAP_SIZE] -= allocated;
        sbrk(-allocated);
        return true;
    }
    return false;
}

/**
 * Detach specified block from its neighbors.
 *
 * @param   block   Pointer to block to detach.
 * @return  Pointer to detached block.
 **/
Block* block_detach(Block *block) {
    Block* before = block->prev;
    Block* after = block->next;

    before->next = after;
    after->prev = before;

    block->next = block;
    block->prev = block;

    return block;
}

/**
 * Attempt to merge source block into destination.
 *
 *  1. Compute end of destination and start of source.
 *
 *  2. If they both match, then merge source into destination by giving the
 *  destination all of the memory allocated to source.
 *
 *  3. If destination is not already in the list, insert merged destination
 *  block into list by updating appropriate references.
 *
 * @param   dst     Destination block we are merging into.
 * @param   src     Source block we are merging from.
 * @return  Whether or not the merge completed successfully.
 **/
bool block_merge(Block *dst, Block *src) {
    char* dst_end = (char*) dst;
    dst_end += sizeof(Block) + dst->capacity;

    if(dst_end == (char*) src) {
        // Give dst all of the memory (including headers) from source
        dst->capacity += sizeof(Block) + src->capacity;
        // Update counters
        Counters[MERGES]++;
        Counters[BLOCKS]--;
    }
    else {
        // Merge failed
        return false;
    }
    // If destination is not in the list, i.e. its next and prev pointers are itself
    if(dst->prev == dst) {
        src->prev->next = dst;
        src->next->prev = dst;
        dst->prev = src->prev;
        dst->next = src->next;
    }
    return true;
}

/**
 * Attempt to split block with the specified size:
 *
 *  1. Check if block capacity is sufficient for requested aligned size and
 *  header Block.
 *
 *  2. Split specified block into two blocks.
 *
 * @param   block   Pointer to block to split into two separate blocks.
 * @param   size    Desired size of the first block after split.
 * @return  Pointer to original block (regardless if it was split or not).
 **/
Block* block_split(Block* block, size_t size) {
    // Check if capacity is large enough to split
    if(block->capacity > sizeof(Block)+ALIGN(size)) {
        // Calculate location of new block
        char* new_block_char = (char*) block;
        new_block_char += sizeof(Block) + ALIGN(size);
        Block* new_block = (Block*) new_block_char;
        // Update new block attributes
        new_block->capacity = block->capacity-(sizeof(Block)+ALIGN(size));
        new_block->size = new_block->capacity;            
        new_block->prev = block;
        new_block->next = block->next;
        // Update block attributes
        block->capacity = ALIGN(size);
        block->size = size;             
        block->next->prev = new_block;
        block->next = new_block;

        Counters[SPLITS]++;
        Counters[BLOCKS]++;
    }
    return block;
}