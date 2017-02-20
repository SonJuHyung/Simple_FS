/*
*	Operating System Lab
*	    Lab4 (File System in Userspace)
*	    Copyright (C) 2017 Juhyung Son <tooson9010@gmail.com>
*	    First Writing: 30/12/2016
*
*   bmap.c :
*       - bitmap related operatios.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/
#include <fuse.h>
#include <ulockmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/file.h> 

#include "include/lab4_fs_types.h"

int alloc_block_inbmap(IOM *iom, long *blocknr_inbmap){

    int res;

    res =scan_allocate_dbmap(iom, blocknr_inbmap);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    
    return LAB4_SUCCESS;

}

int scan_allocate_ibmap(IOM *iom, inode_t hint_free_inode ,inode_t *new_ino){

    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    unsigned int ibitmap_start = sbi->ibitmap_start;    
    unsigned int ibitmap_size = sbi->ibitmap_size;
    unsigned int inode_per_seg = ibitmap_size / LAB4_INODE_ENTRY_SIZE;
    int res = LAB4_ERROR, found = 0;
    unsigned int count=0;
    inode_t free_inode = hint_free_inode;
    char* buf_ibmap;    

    buf_ibmap = iom->iom_buf_ibmap;
    while(count < inode_per_seg){
        if(!lab4_test_bit(free_inode,buf_ibmap)){
            found = 1;
            break;
        }
        free_inode = (free_inode+1) % inode_per_seg;
        count++;
    }

    if(found && free_inode < inode_per_seg){
        lab4_set_bit(free_inode, buf_ibmap);        
        *new_ino = free_inode;
    }else{
        goto ERROR;
    }
    return LAB4_SUCCESS;

ERROR:
    free(buf_ibmap);
    return LAB4_ERROR; 
}

int scan_allocate_dbmap(IOM *iom, long *new_blocknr_inbmap){
    
    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    unsigned int dbitmap_start = sbi->dbitmap_start;    
    unsigned int dbitmap_size = sbi->dbitmap_size;

    int max_block = sbi->darea_size / sbi->cluster_size;
    int res = LAB4_ERROR, count = 0, found = 0;
    long free_block = sbi->last_allocated_blknr;
    char* buf_dbmap;    

    buf_dbmap = iom->iom_buf_dbmap;
    while(count < max_block){
        if(!lab4_test_bit(free_block, buf_dbmap)){
            found = 1;
            break;
        }
        free_block = (free_block+1) % max_block;
        count++;
    }

    if(found && free_block < max_block){
        lab4_set_bit(free_block, buf_dbmap);      
        *new_blocknr_inbmap = free_block;
        dec_free_blocks(iom);
    }else{
        goto ERROR;
    }
    return LAB4_SUCCESS;

ERROR:
    free(buf_dbmap);
    return LAB4_ERROR; 
      
}

void scan_delete_dbmap(IOM *iom, int blocknr_inbmap){

    char* buf_dbmap;    

    buf_dbmap = iom->iom_buf_dbmap;
    lab4_clear_bit(blocknr_inbmap, buf_dbmap);        
}


void scan_delete_ibmap(IOM *iom, inode_t del_ino){

    char* buf_ibmap;    

    buf_ibmap = iom->iom_buf_ibmap;
    lab4_clear_bit(del_ino, buf_ibmap);        
}

