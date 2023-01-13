/* fs.c: SimpleFS file system */

#include "sfs/fs.h"
#include "sfs/logging.h"
#include "sfs/utils.h"

#include <stdio.h>
#include <string.h>

/* External Functions */

/**
 * Debug FileSystem by doing the following:
 *
 *  1. Read SuperBlock and report its information.
 *
 *  2. Read Inode Table and report information about each Inode.
 *
 * @param       disk        Pointer to Disk structure.
 **/
void fs_debug(Disk *disk) {
    Block block;
    /* Read SuperBlock */
    if (disk_read(disk, 0, block.data) == DISK_FAILURE) {
        return;
    }
    printf("SuperBlock:\n");
    printf("    magic number is %s\n",
        (block.super.magic_number == MAGIC_NUMBER) ? "valid" : "invalid");
    printf("    %u blocks\n"         , block.super.blocks);
    printf("    %u inode blocks\n"   , block.super.inode_blocks);
    printf("    %u inodes\n"         , block.super.inodes);

    /* Read Inodes */
    int iblocks = block.super.inode_blocks;
    for(int i = 1; i <= iblocks; i++) {
        Block iblock;
        // Read in the next inode block and check if the read succeedes
        if(disk_read(disk, i, iblock.data) == DISK_FAILURE) { return; }

        // Number if inodes in current block
        int curr_inodes;
        if(iblocks == i) {
            curr_inodes = block.super.inodes-((i-1)*INODES_PER_BLOCK);
        }
        else {
            curr_inodes = INODES_PER_BLOCK;
        }

        for(int j = 0; j < curr_inodes; j++) {
            Inode curr_inode = iblock.inodes[j];

            if(!curr_inode.valid) { continue; }

            printf("Inode %d:\n", INODES_PER_BLOCK*(i-1)+j);
            printf("    size: %u bytes\n", curr_inode.size);

            printf("    direct blocks:");
            for(int k = 0; k < POINTERS_PER_INODE; k++) {
                if(curr_inode.direct[k]) {
                    printf(" %d", curr_inode.direct[k]);
                }
            }
            printf("\n");

            if(curr_inode.indirect) {
                printf("    indirect block: %u\n", curr_inode.indirect);

                Block indirect_block;

                if(disk_read(disk, curr_inode.indirect, indirect_block.data) == DISK_FAILURE) { return; }

                printf("    indirect data blocks:");
                for(int z = 0; z < INODES_PER_BLOCK; z++) {
                    if(indirect_block.pointers[z]) {
                        printf(" %u", indirect_block.pointers[z]);
                    }
                }
                printf("\n");
            }
        }
    }
}

/**
 * Format Disk by doing the following:
 *
 *  1. Write SuperBlock (with appropriate magic number, number of blocks,
 *  number of inode blocks, and number of inodes).
 *
 *  2. Clear all remaining blocks.
 *
 * Note: Do not format a mounted Disk!
 *
 * @param       fs      Pointer to FileSystem structure.
 * @param       disk    Pointer to Disk structure.
 * @return      Whether or not all disk operations were successful.
 **/
bool fs_format(FileSystem *fs, Disk *disk) {
    // If free_blocks are allocated, fs is mounted!
    if(fs->free_blocks != NULL) { return false; }

    Block empty_block = {{0}}; // Empty block data to write to every block
    fs->meta_data.magic_number = MAGIC_NUMBER;
    fs->meta_data.blocks = disk->blocks;

    // InodeBlocks should be '10% of the amount of blocks, rounding up'
    if(fs->meta_data.blocks % 10 == 0) // If superblocks divide by 10, no rounding needed {
        fs->meta_data.inode_blocks = fs->meta_data.blocks / 10;
    }
    //Otherwise, add 1 to the result of the division
    else {
        fs->meta_data.inode_blocks = (fs->meta_data.blocks / 10) + 1;
    }
    fs->meta_data.inodes = fs->meta_data.inode_blocks * INODES_PER_BLOCK;

    for(int i = 1; i < fs->meta_data.blocks; i++) {
        if(disk_write(disk,i,empty_block.data) != BLOCK_SIZE) { return false; }
    }
    return true;
}

/**
 * Mount specified FileSystem to given Disk by doing the following:
 *
 *  1. Read and check SuperBlock (verify attributes).
 *
 *  2. Verify and record FileSystem disk attribute.
 *
 *  3. Copy SuperBlock to FileSystem meta data attribute
 *
 *  4. Initialize FileSystem free blocks bitmap.
 *
 * Note: Do not mount a Disk that has already been mounted!
 *
 * @param       fs      Pointer to FileSystem structure.
 * @param       disk    Pointer to Disk structure.
 * @return      Whether or not the mount operation was successful.
 **/
bool fs_mount(FileSystem *fs, Disk *disk) {
    
    // If free_blocks are allocated, fs is mounted!
    if(fs->free_blocks != NULL) { return false; }
    // Check that the disk is valid
    if(!disk) { return false; }
    // Try to read the superblock
    Block sb;
    if(disk_read(disk, 0, sb.data) == DISK_FAILURE) { return false; }
    // Check magic number
    if(sb.super.magic_number != MAGIC_NUMBER) { return false; }
    // Check blocks
    if(sb.super.blocks != disk->blocks) { return false; }
    // Check inode blocks
    if(sb.super.inode_blocks != sb.super.blocks / 10 + (sb.super.blocks % 10 != 0)) { return false; }
    // Check inodes
    if(sb.super.inodes != sb.super.inode_blocks * INODES_PER_BLOCK) { return false; }

    fs->disk = disk;
    fs->meta_data = sb.super;

    Block indirect_blk,inode_blk;
    fs_initialize_free_block_bitmap(fs,&sb);
    // Walk through the inode table, adjusting bitmap as we go
    for(int i = 1; i <= sb.super.inode_blocks; i++) { // For every inode block
        disk_read(disk,i, inode_blk.data); // Read current inode block
        for(int j = 0; j < INODES_PER_BLOCK; j++) { // For every inode in inode block
            Inode inode = inode_blk.inodes[j];
            if(inode.valid) {
                for(int k = 0; k<POINTERS_PER_INODE; k++) { // For every pointer in the inode
                    if(inode.direct[k]) { // set all direct pointers to false
                        fs->free_blocks[inode.direct[k]] = false;
                    }
                }
                if(inode.indirect) { // If there is an inode block, check all of its values and set those to false
                    fs->free_blocks[inode.indirect] = false; // Indirect block
                    if(disk_read(disk,inode.indirect,indirect_blk.data) == DISK_FAILURE) { return false; }

                    for(int l = 0; l < POINTERS_PER_BLOCK; l++) { // For every point in the referenced indirect block, set false
                        if(indirect_blk.pointers[l]) {
                            fs->free_blocks[indirect_blk.pointers[l]] = false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

/**
 * Unmount FileSystem from internal Disk by doing the following:
 *
 *  1. Set FileSystem disk attribute.
 *
 *  2. Release free blocks bitmap.
 *
 * @param       fs      Pointer to FileSystem structure.
 **/
void fs_unmount(FileSystem *fs) {
    fs->disk = NULL;
    free(fs->free_blocks);
    fs->free_blocks = NULL;
}

/**
 * Allocate an Inode in the FileSystem Inode table by doing the following:
 *
 *  1. Search Inode table for free inode.
 *
 *  2. Reserve free inode in Inode table.
 *
 * Note: Be sure to record updates to Inode table to Disk.
 *
 * @param       fs      Pointer to FileSystem structure.
 * @return      Inode number of allocated Inode.
 **/
ssize_t fs_create(FileSystem *fs) {
    Block block = {{0}};

    for(int i=1;i<=fs->meta_data.inode_blocks;i++) {
        if(disk_read(fs->disk,i,block.data)==BLOCK_SIZE) {
            for(int j=0;j<INODES_PER_BLOCK;j++) {
                if(!block.inodes[j].valid) {
                    block.inodes[j].valid=1;
                    if(disk_write(fs->disk,i,block.data)==BLOCK_SIZE) {
                        return j+((i-1)*INODES_PER_BLOCK); // i is the inode block, so you must multiply by 128
                    }
                }
            }
        }
    }
    return -1;
}

/**
 * Remove Inode and associated data from FileSystem by doing the following:
 *
 *  1. Load and check status of Inode.
 *
 *  2. Release any direct blocks.
 *
 *  3. Release any indirect blocks.
 *
 *  4. Mark Inode as free in Inode table.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to remove.
 * @return      Whether or not removing the specified Inode was successful.
 **/
bool    fs_remove(FileSystem *fs, size_t inode_number) {
    Inode iblank = {0},  inode;
    Block blank = {{0}}, block;

    if(!fs_load_inode(fs, inode_number, &inode)) { return false; }
    // Free direct blocks
    for(int j=0;j<POINTERS_PER_INODE;j++) {
        if(inode.direct[j]) {
            if(disk_write(fs->disk, inode.direct[j], blank.data) == DISK_FAILURE) { return false; }
            fs->free_blocks[inode.direct[j]] = true;
        }
    }

    if(inode.indirect) {
        fs->free_blocks[inode.indirect] = true; // Indirect block
        if(disk_read(fs->disk,inode.indirect,block.data) == DISK_FAILURE) { return false; }

        for(int l=0; l<POINTERS_PER_BLOCK; l++) { // For every point in the referenced indirect block, set true
            if(block.pointers[l]) {
                if(disk_write(fs->disk, block.pointers[l], blank.data) == DISK_FAILURE) { return false; }
                fs->free_blocks[block.pointers[l]] = true;
                block.pointers[l]=0;
            }
        }
        if(disk_write(fs->disk, inode.indirect, blank.data) == DISK_FAILURE) { return false; }
    }
    // write blank over the inode
    fs_save_inode(fs, inode_number, &iblank);
    return true;
}

/**
 * Return size of specified Inode.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to remove.
 * @return      Size of specified Inode (-1 if does not exist).
 **/
ssize_t fs_stat(FileSystem *fs, size_t inode_number) {
    Inode inode;

    if(!fs_load_inode(fs,inode_number, &inode)) { return -1; }

    return inode.size;
}
/**
 * Read from the specified Inode into the data buffer exactly length bytes
 * beginning from the specified offset by doing the following:
 *
 *  1. Load Inode information.
 *
 *  2. Continuously read blocks and copy data to buffer.
 *
 *  Note: Data is read from direct blocks first, and then from indirect blocks.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to read data from.
 * @param       data            Buffer to copy data to.
 * @param       length          Number of bytes to read.
 * @param       offset          Byte offset from which to begin reading.
 * @return      Number of bytes read (-1 on error).
 **/
ssize_t fs_read(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {
    Inode inode;
    if(!fs_load_inode(fs,inode_number,&inode)) { return -1; }

    size_t bytes_read = 0;
    size_t curr_iblk = offset/BLOCK_SIZE;
    size_t block_pos = offset % BLOCK_SIZE;
    // Edge case checks
    if(inode_number>=fs->meta_data.inode_blocks * INODES_PER_BLOCK) { return -1; }

    if(!fs->free_blocks) { return -1; }

    if(curr_iblk < POINTERS_PER_INODE) {
        // Read in the block ontop of which we will read new data
        Block read_block;
        if(disk_read(fs->disk, inode.direct[curr_iblk], read_block.data)==DISK_FAILURE) { return -1; }
        ssize_t to_read = BLOCK_SIZE;
        // Figure out how many bytes we should read in this block
        if(to_read > inode.size - offset) {
            to_read = inode.size - offset;
        }
        // Read block data into *data
        memcpy(data+bytes_read, read_block.data+block_pos, to_read);
        // Update bytes read and see if we have finished
        bytes_read += to_read;

        if(bytes_read == length){return bytes_read;}
        // Indicates we have dealt with starting the read in the middle of a block, i.e. we finished the first read
        block_pos = 0;
    }
    else {
        if(inode.indirect) {
            // Load the indirect pointers block
            Block ind_blk;
            if(disk_read(fs->disk, inode.indirect, ind_blk.data)==DISK_FAILURE) { return -1; }
            // Loop oveer the entries in the indirect block
            if(ind_blk.pointers[curr_iblk-POINTERS_PER_INODE]) {
                Block read_block;
                if(disk_read(fs->disk, ind_blk.pointers[curr_iblk-POINTERS_PER_INODE], read_block.data)==DISK_FAILURE) { return -1; }
                ssize_t to_read = BLOCK_SIZE;
                // Figure out how many bytes we should read in this block
                if(to_read > inode.size - offset) {
                    to_read = inode.size - offset;
                }
                // Read data from block into buffer
                memcpy(data+bytes_read, read_block.data+block_pos, to_read);
                // Update bytes read and see if we have finished
                bytes_read += to_read;
                if(bytes_read == length) { return bytes_read; }
                // Indicates we have dealt with starting the read in the middle of a block, i.e. we finished the first read
                block_pos = 0;
            }
        }
        else {
            return -1;
        }
    }
    return bytes_read;
}

// Find the next free block, return 0 if there isn't one
ssize_t fs_find_free(FileSystem* fs) {
    Block blank = {{0}};
    for(int i = 0; i < fs->meta_data.blocks; i++) {
        // If block i is free
        if(fs->free_blocks[i]) {
            fs->free_blocks[i] = false;
            if(disk_write(fs->disk,i,blank.data) == DISK_FAILURE) { return false; }
            return i;
        }
    }
    return 0;
}

/**
 * Write to the specified Inode from the data buffer exactly length bytes
 * beginning from the specified offset by doing the following:
 *
 *  1. Load Inode information.
 *
 *  2. Continuously copy data from buffer to blocks.
 *
 *  Note: Data is read from direct blocks first, and then from indirect blocks.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to write data to.
 * @param       data            Buffer with data to copy
 * @param       length          Number of bytes to write.
 * @param       offset          Byte offset from which to begin writing.
 * @return      Number of bytes read (-1 on error).
 **/
ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {
    size_t bytes_written = 0;
    size_t curr_iblk = offset/BLOCK_SIZE;
    size_t block_pos = offset % BLOCK_SIZE;

    Inode inode;
    if(!fs_load_inode(fs,inode_number,&inode)) { return -1; }
    // If we need to start writing to the direct blocks
    if(curr_iblk < POINTERS_PER_INODE) {
        // Start writing to direct blocks
        for(int i = curr_iblk; i < POINTERS_PER_INODE; i++) {
            // Check if the direct block has been allocated already
            if(!inode.direct[i]) {
                // Try to find the next free_block and update the direct pointer entry
                ssize_t next_free = fs_find_free(fs);
                if(!next_free) { goto END; }
                inode.direct[i] = next_free;
            }
            // Read in the block ontop of which we will write new data
            Block old_data;
            disk_read(fs->disk, inode.direct[i], old_data.data);
            ssize_t to_write;
            // Figure out how many bytes we should write in this block
            if(BLOCK_SIZE-block_pos <= length-bytes_written) { to_write = BLOCK_SIZE-block_pos; }
            else { to_write = length-bytes_written; }
            // Write necessary data to staging block
            memcpy(old_data.data, data+bytes_written, to_write);
            // Write the updated staging block to memory
            disk_write(fs->disk, inode.direct[i], old_data.data);
            // Update bytes written and see if we have finished
            bytes_written += to_write;
            if(bytes_written == length) { goto END; }
            // Indicates we have dealt with starting the write in the middle of a block, i.e. we finished the first write
            block_pos = 0;
        }
        curr_iblk = POINTERS_PER_INODE;
    }

    /* If we make it here then we either: will start with the indirect blocks, or didn't finish in the direct blocks */

    // Allocate an indirect block if there isn't already one allocated
    if(!inode.indirect) {
        ssize_t next_free = fs_find_free(fs);
        if(!next_free) { goto END; }
        inode.indirect = next_free;
    }
    // Load the indirect pointers block
    Block ind_blk;
    disk_read(fs->disk, inode.indirect, ind_blk.data);
    // Loop oveer the entries in the indirect block
    for(int i = curr_iblk-POINTERS_PER_INODE; i < POINTERS_PER_BLOCK; i++) {
        // Allocate an indirect block if there isn't one already allocated
        if(!ind_blk.pointers[i]) {
            ssize_t next_free = fs_find_free(fs);
            if(!next_free) { goto END; }
            ind_blk.pointers[i] = next_free;
            disk_write(fs->disk, inode.indirect, ind_blk.data);
        }
        // Read in the block ontop of which we will write new data
        Block old_data;
        disk_read(fs->disk, ind_blk.pointers[i], old_data.data);
        ssize_t to_write;
        // Figure out how many bytes we should write in this block
        if(BLOCK_SIZE-block_pos <= length-bytes_written) { to_write = BLOCK_SIZE-block_pos; }
        else { to_write = length-bytes_written; }
        // Write necessary data to staging block
        memcpy(old_data.data, data+bytes_written, to_write);
        // Write the updated staging block to memory
        disk_write(fs->disk, ind_blk.pointers[i], old_data.data);
        // Update bytes written and see if we have finished
        bytes_written += to_write;
        if(bytes_written == length) { goto END; }
        // Indicates we have dealt with starting the write in the middle of a block, i.e. we finished the first write
        block_pos = 0;
    }
END:
    inode.size += bytes_written;
    fs_save_inode(fs,inode_number,&inode);
    return bytes_written;
}

bool fs_load_inode(FileSystem *fs, size_t inode_number, Inode *node) {
    // Plus 1 to shift for superblock
    size_t block_num = inode_number/INODES_PER_BLOCK + 1;
    size_t block_index = inode_number - (block_num-1)*INODES_PER_BLOCK;

    Block ib;
    if(disk_read(fs->disk, block_num, ib.data) == DISK_FAILURE) { return false; }
    // set use Inode pointer to pass the found node back
    *node = ib.inodes[block_index];
    if (!node->valid) return false; 
    return true;
}

bool fs_save_inode(FileSystem *fs, size_t inode_number, Inode *node) {
    // Plus 1 to shift for superblock
    size_t block_num = inode_number/INODES_PER_BLOCK + 1;
    size_t block_index = inode_number - (block_num-1)*INODES_PER_BLOCK;

    Block ib;
    if(disk_read(fs->disk, block_num, ib.data) == DISK_FAILURE) { return false; }
    // Update the inode block with the new inode
    ib.inodes[block_index] = *node;
    // Write back the updated block
    if(disk_write(fs->disk, block_num, ib.data) == DISK_FAILURE) { return false; }

    return true;
}

void fs_initialize_free_block_bitmap(FileSystem *fs, Block *sb){
    fs->free_blocks = calloc(1, sb->super.blocks);
    for(int i=0;i<sb->super.blocks;i++) {
        fs->free_blocks[i] = true; 
        if(i<=sb->super.inode_blocks) {
            fs->free_blocks[i] = false;
        }
    }
}