/*
*	Operating System Lab
*	    Lab4 (File System in Userspace)
*	    Copyright (C) 2017 Juhyung Son <tooson9010@gmail.com>
*	    First Writing: 30/12/2016
*
*   inode.c :
*       - inode related operatios.
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

int do_path_check(const char *path)
{
	char *basename_path;
	basename_path = strrchr(path, '/');
	if (basename_path == NULL) {
		return LAB4_ERROR;
	}
//	basename_path++;

	return LAB4_SUCCESS;
}


static inline u16 old_encode_dev(dev_t dev)
{
	return (major(dev) << 8) | minor(dev);
}

static inline u32 new_encode_dev(dev_t dev)
{
	unsigned major = major(dev);
	unsigned minor = minor(dev);
	return (minor & 0xff) | (major << 8) | ((minor & ~0xff) << 12);
}

static inline int old_valid_dev(dev_t dev)
{
	return major(dev) < 256 && minor(dev) < 256;
}

static inline dev_t old_decode_dev(u16 val)
{
	return makedev((val >> 8) & 255, val & 255);
}

static inline dev_t new_decode_dev(u32 dev)
{
	unsigned major = (dev & 0xfff00) >> 8;
	unsigned minor = (dev & 0xff) | ((dev >> 12) & 0xfff00);
	return makedev(major, minor);
}



int do_fill_inode(IOM *iom, INODE *inode, struct stat *stbuf){

    stbuf->st_mode	= inode->i_mode;
    stbuf->st_nlink	= inode->i_links_count;
    stbuf->st_size	= inode->i_size;
    stbuf->st_atime	= inode->i_atime;
    stbuf->st_mtime	= inode->i_mtime;
    stbuf->st_ctime	= inode->i_ctime;
    stbuf->st_gid	= inode->i_gid;
    stbuf->st_uid	= inode->i_uid;

    if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode)) {
        stbuf->st_rdev = old_decode_dev(inode->i_blocks[0]);
    } else {
        stbuf->st_rdev = new_decode_dev(inode->i_blocks[1]);
    }


    return LAB4_SUCCESS;
}

INODE* do_read_inode(IOM *iom, inode_t ino){

    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    INODE *inode = NULL, *tmp_inode = NULL;
	int blocknr;
	int offset;
    char *buf_itble;
	
	if (ino < LAB4_ROOT_INO)
		return NULL;

    
    inode = (struct lab4_inode*)malloc(LAB4_INODE_ENTRY_SIZE);
    if(!inode)
        return NULL;
    memset(inode, 0x0, LAB4_INODE_ENTRY_SIZE);

    buf_itble = iom->iom_buf_itble;
    tmp_inode = (INODE*)buf_itble;
    memcpy(inode, &tmp_inode[ino], LAB4_INODE_ENTRY_SIZE);
    
    return inode;
}

inode_t get_root_ino(struct lab4_sb_info *sbi){
    return sbi->root_ino;
}

int lab4_read_inode(IOM *iom, const char *path, INODE **inode){
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    struct lab4_dir_entry dir_entry;
    INODE *tmp_inode=NULL;
    inode_t cur_ino;
    char tmp_path[256];
    char* filename;
    int res,isdelete=0;

    cur_ino = get_root_ino(sbi);

    memset(tmp_path, 0x0, 256);
    if(strcmp(path,"/")){
      
        strcpy(tmp_path,path);

        filename = strtok(tmp_path,"/");

        while(filename != NULL)
        {                
            res = lab4_lookup(iom, cur_ino, &dir_entry,filename,&isdelete);
            if(res == LAB4_ERROR)
                return LAB4_ERROR;
            if(res == -ENOENT)
                return -ENOENT;

            cur_ino = dir_entry.d_ino;        

            filename = strtok(NULL,"/");
            if(filename == NULL)
                break;
            cur_ino = dir_entry.d_ino;        
            memset(&dir_entry, 0x0, LAB4_DIR_ENTRY_SIZE);
        }        
    }        
    *inode = do_read_inode(iom,cur_ino);

    if(!inode)
        return LAB4_ERROR;
    
    return LAB4_SUCCESS;
}

int update_sb(IOM *iom){
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int res;

    res = pwrite(iom->dev_fd, iom->sb ,sbi->cluster_size,sbi->sb_start);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    return LAB4_SUCCESS;
}

int update_ibmap(IOM *iom){
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int res;

    res = pwrite(iom->dev_fd, iom->iom_buf_ibmap ,sbi->cluster_size, sbi->ibitmap_start);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    return LAB4_SUCCESS;
}

int update_dbmap(IOM *iom){
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int res;

    res = pwrite(iom->dev_fd, iom->iom_buf_dbmap ,sbi->cluster_size, sbi->dbitmap_start);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    return LAB4_SUCCESS;
}

int update_itble(IOM *iom){
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int res;

    res = pwrite(iom->dev_fd, iom->iom_buf_itble ,sbi->cluster_size, sbi->itable_start);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    return LAB4_SUCCESS;
}

int restore_sb(IOM *iom){
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int res;

    res = pread(iom->dev_fd, iom->sb,sbi->cluster_size,sbi->sb_start);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    free(sbi);
    lab4_fill_super(iom);

    return LAB4_SUCCESS;
}

int restore_ibmap(IOM *iom){
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int res;

    res = pread(iom->dev_fd, iom->iom_buf_ibmap ,sbi->cluster_size, sbi->ibitmap_start);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    return LAB4_SUCCESS;
}

int restore_dbmap(IOM *iom){
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int res;

    res = pread(iom->dev_fd, iom->iom_buf_dbmap ,sbi->cluster_size, sbi->dbitmap_start);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    return LAB4_SUCCESS;
}

int restore_itble(IOM *iom){
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int res;

    res = pread(iom->dev_fd, iom->iom_buf_itble ,sbi->cluster_size, sbi->itable_start);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    return LAB4_SUCCESS;
}

void inc_free_inodes(IOM *iom){
    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);

    sbi->sb_free_inodes++;
    sb->sb_free_inodes++;
}


void dec_free_inodes(IOM *iom){
    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);

    sbi->sb_free_inodes--;
    sb->sb_free_inodes--;
}

void inc_free_blocks(IOM *iom){
    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);

    sbi->sb_free_blocks++;
    sb->sb_free_blocks++;
}


void dec_free_blocks(IOM *iom){
    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);

    sbi->sb_free_blocks--;
    sb->sb_free_blocks--;
}

INODE* alloc_new_inode(IOM *iom){
    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    struct lab4_inode *new_inode;
    inode_t last_allocated_ino = sbi->last_allocated_ino;
    int res;
    inode_t new_ino;

    res = scan_allocate_ibmap(iom,last_allocated_ino,&new_ino);
    if(res == LAB4_ERROR)
        return NULL;
    
    new_inode = (INODE*)malloc(LAB4_INODE_ENTRY_SIZE);
    memset(new_inode, 0x0, LAB4_INODE_ENTRY_SIZE);

    new_inode->i_ino = new_ino;
    new_inode->i_deleted = 0;
    new_inode->i_version++; 

    sb->sb_last_allocated_ino = new_ino+1;
    sbi->last_allocated_ino = new_ino+1;
    dec_free_inodes(iom);


    return new_inode; 
}

int write_inode_to_itble(IOM *iom, INODE *new_inode){
    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    struct lab4_inode *tmp_inode;
    int res, new_ino_pos;
    char *buf_itble;

    buf_itble = iom->iom_buf_itble;
    tmp_inode = (struct lab4_inode*)buf_itble;

    new_ino_pos = new_inode->i_ino * LAB4_INODE_ENTRY_SIZE;
    if(new_ino_pos > sbi->cluster_size)
        return LAB4_ERROR;

    memcpy(&tmp_inode[new_inode->i_ino], new_inode, LAB4_INODE_ENTRY_SIZE);

    return LAB4_SUCCESS;
}

int delete_inode_from_itble(IOM *iom, INODE *del_inode){
    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    struct lab4_inode *tmp_inode;
    inode_t del_ino_pos;        
    int res;
    char *buf_itble, *buf_zero;

    buf_itble = iom->iom_buf_itble;
    tmp_inode = (struct lab4_inode*)buf_itble;

    del_ino_pos = del_inode->i_ino * LAB4_INODE_ENTRY_SIZE;
    if(del_ino_pos > sbi->cluster_size)
        return LAB4_ERROR;

    memset(&tmp_inode[del_inode->i_ino], 0x0, LAB4_INODE_ENTRY_SIZE);

    return LAB4_SUCCESS;
}

void delete_block_inbmap(IOM *iom, INODE *inode){
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int blocknr_inbmap;
    long del_blocknr = inode->i_blocks[0];
    
    blocknr_inbmap = (del_blocknr - sbi->darea_start)/ sbi->cluster_size;
    scan_delete_dbmap(iom, blocknr_inbmap);
    inc_free_blocks(iom);
}

int restore_data(IOM *iom,char *buf, int size, long blocknr){
    pwrite(iom->dev_fd, buf, size,blocknr);
}

int delete_inode(IOM *iom, INODE *del_inode){

    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int res = LAB4_ERROR, blocknr_inbmap;
    char *buf_del_block;
    long del_blocknr;
    
    scan_delete_ibmap(iom, del_inode->i_ino);

    res = delete_inode_from_itble(iom, del_inode);
    if(res == LAB4_ERROR){
        return LAB4_ERROR;
    } 
    inc_free_inodes(iom);

    return LAB4_SUCCESS;    
}

void inc_link(IOM *iom, inode_t par_ino){
    char *buf_itble = iom->iom_buf_itble; 
    INODE *tmp_inode = (INODE*)buf_itble;

    tmp_inode[par_ino].i_links_count++;
}

void dec_link(IOM *iom, inode_t par_ino){
    char *buf_itble = iom->iom_buf_itble; 
    INODE *tmp_inode = (INODE*)buf_itble;

    tmp_inode[par_ino].i_links_count--;
}

int do_utimens(IOM *iom, const char *path, const struct timespec ts[2])
{
	struct lab4_dir_entry file_entry;
	struct lab4_inode *inode;
	char f_name[FNAME_SIZE] = {0,};
	char d_name[FNAME_SIZE] = {0,};
	int res, isdelete=0;

	if (strcmp(path, "/") == 0) {
		res = -1;
	} else {
        res = do_path_check(path);
        if(res == LAB4_ERROR)
            return LAB4_ERROR;

        get_name_from_path(path, f_name, d_name);

        res = lab4_read_inode(iom, path, &inode);
        if(res == LAB4_ERROR)
            return LAB4_ERROR; 
        else if(res == -ENOENT)
            return -ENOENT;
        
        /* set new access time */
        inode->i_atime = ts[0].tv_sec;
        /* sec new modification time */
        inode->i_mtime = ts[1].tv_sec;
	}

	return LAB4_SUCCESS;
}




