/* posix.c: POSIX API Implementation */

#include "malloc/counters.h"
#include "malloc/freelist.h"

#include <assert.h>
#include <string.h>

/**
 * Allocate specified amount memory.
 * @param   size    Amount of bytes to allocate.
 * @return  Pointer to the requested amount of memory.
 **/
void *malloc(size_t size) {
    // Initialize counters
    init_counters();

    // Handle empty size
    if (!size) {
        return NULL;
    }
    // Search free list for any available block with matching size
    Block* block = free_list_search(size);
    if(!block) {
        // Try to allocate if there isn't a free block
        block = block_allocate(size);
    }
    else {
        // Try to split found entry from the free list
        block = block_split(block, size); 
        block = block_detach(block);
    }
    // Could not find free block or allocate a block, so just return NULL
    if (!block) {
        return NULL;
    }
    block->size = size;     
    // Check if allocated block makes sense
    assert(block->capacity >= block->size);
    assert(block->size     == size);
    assert(block->next     == block);
    assert(block->prev     == block);
    // Update counters
    Counters[MALLOCS]++;
    Counters[REQUESTED] += size;
    // Return data address associated with block
    return block->data;
}

/**
 * Release previously allocated memory.
 * @param   ptr     Pointer to previously allocated memory.
 **/
void free(void *ptr) {
    if (!ptr) {
        return;
    }
    // Update counters
    Counters[FREES]++;

    Block* block_head = BLOCK_FROM_POINTER(ptr);
    // Try to release block, otherwise insert it into the free list
    if(!block_release(block_head)) {
        free_list_insert(block_head);
    }
}

/**
 * Allocate memory with specified number of elements and with each element set
 * to 0.
 * @param   nmemb   Number of elements.
 * @param   size    Size of each element.
 * @return  Pointer to requested amount of memory.
 **/
void *calloc(size_t nmemb, size_t size) {
    size_t truesize = nmemb*size;
    void* ptr = malloc(truesize);
    // Return NULL if malloc fails
    if(!ptr) {
        return NULL;
    }
    memset(ptr,0,truesize);
    Counters[CALLOCS]++;
    return ptr;
}

/**
 * Reallocate memory with specified size.
 * @param   ptr     Pointer to previously allocated memory.
 * @param   size    Amount of bytes to allocate.
 * @return  Pointer to requested amount of memory.
 **/
void *realloc(void *ptr, size_t size) {
    Counters[REALLOCS]++;

    if (!ptr) {
        return malloc(size);
    }
    Block* block = BLOCK_FROM_POINTER(ptr);

    if(block->size >= size) {
        if(size==0) {
            free(ptr);
            return NULL;
        }
        block = block_split(block,size);
        free_list_insert(block->next);
        return block->data;
    }
    void* new_ptr = malloc(size);
    if(!new_ptr) {
        return NULL;
    }
    memcpy(new_ptr,ptr,block->size);
    free(ptr);
    return new_ptr;
}