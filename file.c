/*
*	Operating System Lab
*	    Lab4 (File System in Userspace)
*	    Copyright (C) 2017 Juhyung Son <tooson9010@gmail.com>
*	    First Writing: 30/12/2016
*
*   file.c :
*       - ?
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
#include <dirent.h>

#include "include/lab4_fs_types.h"


int lab4_allocate_open_file_table(IOM *iom)
{
    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    int i = 0, fid = -1;

	for (i = START_OPEN_FILE; i < MAX_OPEN_FILE; i++)
	{
		if (sbi->file_table[i].used == FALSE)
		{
			fid = i;
			break;
		}
	}

	return fid;
}

int lab4_seek(struct lab4_super_block *sb, struct lab4_file_table *ft,long offset, int position)
{	
	if (position == SEEK_SET)             /* SEEK_SET */
		ft->rwoffset = offset;
	else if (position == SEEK_CUR)        /* SEEK_CUR */
		ft->rwoffset += offset;
	else if (position == SEEK_END)        /* SEEK_END */
		ft->rwoffset = ft->size - offset;

	return LAB4_SUCCESS;
}


int lab4_createfile(IOM *iom, INODE *parent_inode,char *filename, INODE **new_inode_ptr, mode_t mode){

    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    DIRENTRY *dir_new_entry, *dir_default_entry ,*tmp_dir_entry, dir_entry;
    INODE *new_inode;
	long blocknr_inbmap=0, read_bytes=0, parent_dir_size=0;
    char *buf_new_dir_area, *buf_parent_dir_area;   
    int cluster_size = sbi->cluster_size, parent_dir_blocknr;
	int res = LAB4_ERROR, isdelete = 0, i;

	if (strlen(filename) < 1 || strlen(filename) >= FNAME_SIZE) {
		return -ENAMETOOLONG;
	}

    res = lab4_lookup(iom, parent_inode->i_ino, &dir_entry,filename,&isdelete);
    if(res == LAB4_SUCCESS)
        return -EEXIST;

    if (parent_inode->i_links_count == MAX_FILES_PER_DIR)
	{
        /* directory full  */
		return LAB4_ERROR;
	}

    new_inode = alloc_new_inode(iom);
    if(new_inode == NULL){
        restore_ibmap(iom);
        return LAB4_ERROR;
    }

	new_inode->i_type = LAB4_TYPE_FILE;
	new_inode->i_size = 0;
	new_inode->i_mode = mode;
	new_inode->i_gid = getgid();
	new_inode->i_uid = getuid();
	new_inode->i_links_count = 1;
	new_inode->i_atime = time(NULL);
	new_inode->i_ctime = time(NULL);
	new_inode->i_mtime = time(NULL);
#if 0
	if (S_ISCHR(mode) || S_ISBLK(mode)) {
		if (old_valid_dev(dev))
			new_inode->i_blocks[0] = old_encode_dev(dev);
		else
			new_inode->i_blocks[1] = new_encode_dev(dev);
	}
#endif
	    
    parent_dir_size = parent_inode->i_size;
    parent_dir_blocknr = parent_inode->i_blocks[0];

    dir_new_entry = (DIRENTRY*)malloc(LAB4_DIR_ENTRY_SIZE);
    if(!dir_new_entry)
        goto LAB4_FREE1;
    memset(dir_new_entry, 0x0, LAB4_DIR_ENTRY_SIZE);

	strcpy(dir_new_entry->d_filename,filename);
	dir_new_entry->d_ino = new_inode->i_ino;
	dir_new_entry->d_flag = LAB4_DIR_USED;
	dir_new_entry->d_version = new_inode->i_version;

    buf_parent_dir_area = (char*)malloc(cluster_size);
    if(!buf_parent_dir_area)
        goto LAB4_FREE2;
    memset(buf_parent_dir_area, 0x0 , cluster_size);

    res = pread(iom->dev_fd, buf_parent_dir_area ,parent_dir_size, parent_dir_blocknr);
    if(res == LAB4_ERROR)
        goto LAB4_FREE2;

    tmp_dir_entry = (struct lab4_dir_entry*)buf_parent_dir_area;
    for (read_bytes = 0; read_bytes < parent_dir_size; read_bytes += LAB4_DIR_ENTRY_SIZE, tmp_dir_entry++) {

        if (tmp_dir_entry->d_flag != LAB4_DIR_USED) {
            break;
        }
    }
    new_inode->i_ptr = read_bytes/LAB4_DIR_ENTRY_SIZE;

    res = write_inode_to_itble(iom, new_inode);
    if(res == LAB4_ERROR){
        restore_itble(iom);
        goto LAB4_FREE2;
    }

    pwrite(iom->dev_fd, dir_new_entry, LAB4_DIR_ENTRY_SIZE, parent_dir_blocknr  + read_bytes );

    dec_free_inodes(iom);
    *new_inode_ptr = new_inode;
    update_sb(iom);    
    update_ibmap(iom);
    update_itble(iom);

    free(buf_parent_dir_area);
    return LAB4_SUCCESS;

LAB4_FREE2:
    free(buf_parent_dir_area);

LAB4_FREE1:
    free(new_inode);

	return LAB4_SUCCESS;

}


int do_openfile(IOM *iom, INODE *parent_inode ,char *filename,int flags,int mode){

    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    struct lab4_dir_entry file_entry;
    struct lab4_inode *file_inode = NULL;	
    struct lab4_file_table *ft;
    int fid = -1, res = 0, isdelete = 0;

    memset(&file_entry, 0x0, LAB4_INODE_ENTRY_SIZE);

    res = lab4_lookup(iom,parent_inode->i_ino,&file_entry,filename,&isdelete);
    if(res == LAB4_ERROR)
        return res;
    else if (res == -ENOENT) 
    { 
        if (flags & O_RDWR || flags & O_CREAT) 
        {
            res = lab4_createfile(iom, parent_inode, filename, &file_inode, mode);
            if (res == LAB4_ERROR)
                return res;
           /*FIXME*/ 
        }
        else {
            fid = -1;
            goto RES;
        }
    }else{
       file_inode = do_read_inode(iom,file_entry.d_ino);
    }

    if (file_inode->i_type != LAB4_TYPE_FILE) {		
        printf("This is not a file");
        fid = -1;
        goto RES;
    }

    fid = lab4_allocate_open_file_table(iom);
    if (fid < 0) {
        goto RES;
    }

    ft = sbi->file_table + fid;

    ft->used = TRUE;
    ft->ino = file_inode->i_ino;
    ft->size = file_inode->i_size;
    ft->rwoffset = 0;
    ft->flags = flags;

    if (O_APPEND & flags)
        lab4_seek(sb, ft, file_inode->i_size, SEEK_SET);
    else
        lab4_seek(sb, ft, 0, SEEK_SET);

RES:
    return (fid);
} 


int lab4_do_open(IOM *iom, const char *path, int flags, int mode){
    
    struct lab4_inode *parent_inode;
    char f_name[FNAME_SIZE]={0.};
    char d_name[FNAME_SIZE]={0,};
	int res = LAB4_ERROR, fd;

    res = do_path_check(path);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;

    get_name_from_path(path, f_name, d_name);

    res = lab4_read_inode(iom, d_name, &parent_inode);
    if(res == -ENOENT)
       return res;
    else if(res == LAB4_ERROR)
        return res;

	fd = do_openfile(iom, parent_inode, f_name, flags, mode);
		
	return fd;
}

void lab4_do_close(IOM *iom, int fid) 
{
	struct lab4_sb_info *sbi = IOM_SB_I(iom);
	struct lab4_file_table *ft;

	ft = sbi->file_table + fid;

	ft->ino = 0;	
	ft->size = 0;
	ft->used = 0;
	ft->rwoffset = 0;
	ft->prefetch_cur = 0;
	ft->flags = 0;

}

int _delete_block(IOM *iom, unsigned int blocknr, int depth){
    int cluster_size = iom->cluster_size, index = 0, res = LAB4_ERROR;
    int *i_blocks;
    int max = cluster_size / 4;
    char buf_zero[cluster_size];
    char buf_block[cluster_size];

    memset(buf_zero, 0x0, cluster_size);
    memset(buf_block, 0x0, cluster_size);

    res = pread(iom->dev_fd, buf_block, cluster_size, blocknr);
    if(res == LAB4_ERROR){
        return res;
    }

    i_blocks = (int*)buf_block;
    if(depth != 0){
        for(index; index < max ; index++ ){            
            _delete_block(iom,i_blocks[index], --depth);
        }
    }else{
        for(index ; index <= max; index++){
            if(i_blocks[index] != 0){
                res = pwrite(iom->dev_fd, buf_zero, cluster_size,i_blocks[index]); 
                if(res == LAB4_ERROR)
                    return res;
            }       
        }        
    }

    return LAB4_SUCCESS;
}

int delete_block(IOM *iom, INODE *del_inode){
    int index=0, type = 1;
    int cluster_size = iom->cluster_size;
    char buf_zero[cluster_size];
    int *i_blocks = del_inode->i_blocks, res= LAB4_ERROR;

    memset(buf_zero, 0x0, cluster_size);

    for(index ; index <= DIRECT_BLOCKS; index++){
        if(i_blocks[index] != 0){
            res = pwrite(iom->dev_fd, buf_zero, cluster_size,i_blocks[index]); 
            if(res == LAB4_ERROR)
                return res;
        }       
    } 
    for(index; index <= TINDIRECT_BLOCKS; index++, type++){
        if(i_blocks[index] != 0)
            _delete_block(iom, i_blocks[index], type);
    }
    return LAB4_SUCCESS; 
}

int do_unlink(IOM *iom, INODE *parent_inode, char *filename)
{
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    struct lab4_dir_entry del_entry;
    INODE *del_inode=NULL;
    inode_t cur_ino;
    char tmp_path[256];
    char* buf_zero= NULL, *buf_backup=NULL;
    int res, cluster_size = sbi->cluster_size, isdelete = 1;
    

    res = lab4_lookup(iom, parent_inode->i_ino, &del_entry,filename, &isdelete);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    if(res == -ENOENT)
        return -ENOENT;

    del_inode = do_read_inode(iom, del_entry.d_ino);
    if(!del_inode)
        goto FREE1;

    del_inode->i_links_count--;
    if(del_inode->i_links_count == 0){
        
        if(del_inode->i_size != 0){

            delete_block_inbmap(iom, del_inode);
            res = delete_block(iom, del_inode);
            if(res == LAB4_ERROR){
                restore_dbmap(iom);
                goto FREE1;
            }
        }

        res = delete_inode(iom, del_inode);
        if(res == LAB4_ERROR){
            restore_ibmap(iom);
            goto FREE1;
        }

    }

    if(isdelete !=-1){
        pwrite(iom->dev_fd, &del_entry,LAB4_DIR_ENTRY_SIZE,parent_inode->i_blocks[0]+isdelete);
    }

    update_sb(iom);
    update_ibmap(iom);
    update_dbmap(iom);
    update_itble(iom);

    free(del_inode);

    return LAB4_SUCCESS;

FREE1:
    free(del_inode);

    return LAB4_ERROR;
}



