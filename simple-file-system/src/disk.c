/* disk.c: SimpleFS disk emulator */

#include "sfs/disk.h"
#include "sfs/logging.h"

#include <fcntl.h>
#include <unistd.h>

/* Internal Prototyes */

bool    disk_sanity_check(Disk *disk, size_t blocknum, const char *data);

/* External Functions */

/**
 *
 * Opens disk at specified path with the specified number of blocks by doing
 * the following:
 *
 *  1. Allocate Disk structure and sets appropriate attributes.
 *
 *  2. Open file descriptor to specified path.
 *
 *  3. Truncate file to desired file size (blocks * BLOCK_SIZE).
 *
 * @param       path        Path to disk image to create.
 * @param       blocks      Number of blocks to allocate for disk image.
 *
 * @return      Pointer to newly allocated and configured Disk structure (NULL
 *              on failure).
 **/
Disk*	disk_open(const char *path, size_t blocks) {
    if(blocks > 1000) return NULL;
    Disk* new_disk = calloc(1,sizeof(Disk));
    // If allocation fails, return NULL
    if(!new_disk) {
        disk_close(new_disk);
        return NULL;
    }
    // Open file descriptor
    int fd = open(path ,O_RDWR | O_CREAT ,0600);
    // If open fails, return NULL
    if(fd == -1) {
        disk_close(new_disk);
        return NULL;
    }
    // Reads and writes already set to 0 by calloc
    new_disk->blocks = blocks;
    new_disk->fd = fd;

    if(truncate(path, blocks*BLOCK_SIZE) == -1) {
        disk_close(new_disk);
        return NULL;
    }
    return new_disk;
}

/**
 * Close disk structure by doing the following:
 *
 *  1. Close disk file descriptor.
 *
 *  2. Report number of disk reads and writes.
 *
 *  3. Release disk structure memory.
 *
 * @param       disk        Pointer to Disk structure.
 */
void	disk_close(Disk *disk) {
    printf("%ld disk block reads\n", disk->reads);
    printf("%ld disk block writes\n", disk->writes);

    close(disk->fd);
    free(disk);
}

/**
 * Read data from disk at specified block into data buffer by doing the
 * following:
 *
 *  1. Perform sanity check.
 *
 *  2. Seek to specified block.
 *
 *  3. Read from block to data buffer (must be BLOCK_SIZE).
 *
 * @param       disk        Pointer to Disk structure.
 * @param       block       Block number to perform operation on.
 * @param       data        Data buffer.
 *
 * @return      Number of bytes read.
 *              (BLOCK_SIZE on success, DISK_FAILURE on failure).
 **/
ssize_t disk_read(Disk *disk, size_t block, char *data) {
    // Check for valid args
    if(!disk_sanity_check(disk, block, data)) {
        return DISK_FAILURE;
    }

    if(lseek(disk->fd, block*BLOCK_SIZE, SEEK_SET) == -1) {
        return DISK_FAILURE;
    }
    // Read bytes; check if size is valid
    if(read(disk->fd, data, BLOCK_SIZE) != BLOCK_SIZE) {
        return DISK_FAILURE;
    }
    // Increment disk reads, return BLOCK_SIZE
    disk->reads++;
    return BLOCK_SIZE;
}

/**
 * Write data to disk at specified block from data buffer by doing the
 * following:
 *
 *  1. Perform sanity check.
 *
 *  2. Seek to specified block.
 *
 *  3. Write data buffer (must be BLOCK_SIZE) to disk block.
 *
 * @param       disk        Pointer to Disk structure.
 * @param       block       Block number to perform operation on.
 * @param       data        Data buffer.
 *
 * @return      Number of bytes written.
 *              (BLOCK_SIZE on success, DISK_FAILURE on failure).
 **/
ssize_t disk_write(Disk *disk, size_t block, char *data) {
    if(!disk_sanity_check(disk, block, data)) {
        return DISK_FAILURE;
    }
    if(lseek(disk->fd, block*BLOCK_SIZE, SEEK_SET) == -1) {
        return DISK_FAILURE;
    }
    // Write bytes; check if size is valid
    if(write(disk->fd, data, BLOCK_SIZE) != BLOCK_SIZE) {
        return DISK_FAILURE;
    }
    disk->writes++;
    return BLOCK_SIZE;
}

/* Internal Functions */

/**
 * Perform sanity check before read or write operation by doing the following:
 *
 *  1. Check for valid disk.
 *
 *  2. Check for valid block.
 *
 *  3. Check for valid data.
 *
 * @param       disk        Pointer to Disk structure.
 * @param       block       Block number to perform operation on.
 * @param       data        Data buffer.
 *
 * @return      Whether or not it is safe to perform a read/write operation
 *              (true for safe, false for unsafe).
 **/
bool disk_sanity_check(Disk *disk, size_t block, const char *data) {
    if(!disk) { 
        return false;
    }
    if(block < 0 || block >= disk->blocks) {
        return false;
    }
    if(!data) {
        return false;
    }
    return true;
}