/*
*	Operating System Lab
*	    Lab4 (File System in Userspace)
*	    Copyright (C) 2017 Juhyung Son <tooson9010@gmail.com>
*	    First Writing: 30/12/2016
*
*   dir.c :
*       - operations for manipulating directories, including adding entries to directories and 
*         walking the directory structure on-disk to access file.
*       - 
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

int lab4_lookup(IOM *iom, inode_t parent_dir_ino, struct lab4_dir_entry *file_entry, const char *filename, int *isdelete){

    struct lab4_sb_info *sbi = IOM_SB_I(iom);
	INODE *dir_inode;
	struct lab4_dir_entry *dir;
	unsigned int offset = 0;
	long read_bytes = 0;
	long dir_size = 0;
	int start = 0;
	int res = -1;
    int blocknr;
    char *buf_dir_entry;

    dir_inode = do_read_inode(iom, parent_dir_ino);
	if (!dir_inode)
		return LAB4_ERROR;

    buf_dir_entry = (char*)malloc(iom->cluster_size);
    if(!buf_dir_entry)
        return LAB4_ERROR;
    memset(buf_dir_entry, 0x0, iom->cluster_size);

	dir_size = dir_inode->i_size;
	start = offset * LAB4_DIR_ENTRY_SIZE;
    blocknr = dir_inode->i_blocks[0];

    res = pread(iom->dev_fd,buf_dir_entry, iom->cluster_size, blocknr);
    if(res ==  LAB4_ERROR)
        goto FREE;
    dir = (struct lab4_dir_entry *)buf_dir_entry;

    /*FIXME - indirect block, triple block support, no such file */
	for (read_bytes = start; read_bytes < dir_size; read_bytes += LAB4_DIR_ENTRY_SIZE) {

		if (dir->d_flag == LAB4_DIR_USED) {
			if (!strcmp(dir->d_filename, filename)) {
                memcpy(file_entry, dir, LAB4_DIR_ENTRY_SIZE);
                if(*isdelete == 1){
                    file_entry->d_flag = LAB4_DIR_DELETED;
                    *isdelete = read_bytes;
                }
                else{
                   *isdelete = -1; 
                }
				return LAB4_SUCCESS;
			}
		}
		dir++;
	}
NOENT:
    free(buf_dir_entry);
    free(dir_inode);
    return -ENOENT; 

FREE:
    free(buf_dir_entry);
    free(dir_inode);
    return LAB4_ERROR;
}

int get_name_from_path(const char *path, char *f_name, char* d_name)
{   
    char *f_ptr = NULL,*f_tmp=NULL ,*tmp_path = NULL;
    int len, f_start;
   
    len = strlen(path);
    tmp_path = (char*)malloc(len);
    strcpy(tmp_path, path);

#if 0 
    f_ptr = strtok(tmp_path, "/");
    while(f_ptr != NULL){
        if(!strchr(f_ptr, '/')){
            strcpy(f_name, f_ptr);
            break;
        }
        f_ptr = strtok(NULL,"/");
    }
    strncpy(d_name, tmp_path, f_ptr-tmp_path);
#endif 
#if 1
    f_ptr = tmp_path;
    while(f_ptr != NULL){

        f_tmp = strchr(f_ptr, '/');
        if(!f_tmp){
            len = strlen(f_ptr);
            strncpy(f_name, f_ptr,len);
            break;
        }
        f_ptr = f_tmp+1;
        //f_ptr = strtok(NULL,"/");
    }
    strncpy(d_name, tmp_path, (f_ptr-1)-tmp_path);
#endif 
#if 0
    f_ptr = strrchr(tmp_path,'/');
    strcpy(f_name, f_ptr+1);
    strncpy(d_name, tmp_path, f_ptr-tmp_path);
#endif
    return LAB4_SUCCESS;
}


int do_readdir(IOM *iom,INODE* dir_inode, int offset,void* buf, fuse_fill_dir_t filler)
{
    struct stat st;
    DIRENTRY *dir_entry;
    INODE *entry_inode;
	long dir_size = 0, blocknr=0, start = 0;
	int res = LAB4_ERROR;
    int read_bytes=0;
    char *buf_dir_entry;

	dir_size = dir_inode->i_size;
    blocknr = dir_inode->i_blocks[0];
    start = offset * LAB4_DIR_ENTRY_SIZE;

    buf_dir_entry = (char*)malloc(dir_size);
    if(!buf_dir_entry)
        return LAB4_ERROR;
    memset(buf_dir_entry, 0x0, dir_size);

    res = pread(iom->dev_fd, buf_dir_entry,dir_size,blocknr);
    if(res == LAB4_ERROR)
        goto LAB4_FREE;

    dir_entry = (struct lab4_dir_entry*)buf_dir_entry;

    for (read_bytes = start; read_bytes < dir_size; read_bytes += LAB4_DIR_ENTRY_SIZE, dir_entry++) {

        if (dir_entry->d_flag == LAB4_DIR_USED) {

            memset(&st, 0, sizeof(st));

            entry_inode = do_read_inode(iom,dir_entry->d_ino);
            if(entry_inode->i_type == LAB4_TYPE_DIRECTORY)
                st.st_mode = DT_DIR << 12;
            else 
                st.st_mode = DT_REG << 12;
                
            st.st_ino =  dir_entry->d_ino;            
            //st.st_mode = S_IFDIR | 0755;
            st.st_nlink = 2;

            filler(buf, dir_entry->d_filename, &st,0);
        }
    }

    free(buf_dir_entry);
    return LAB4_SUCCESS;
LAB4_FREE:
    free(buf_dir_entry);
    return LAB4_ERROR;
}

int dir_is_invalid(struct lab4_dir_entry *dir){
    if(dir->d_flag == LAB4_DIR_EMPTY || dir->d_flag == LAB4_DIR_DELETED)
        return 1;
    return 0;
}

int do_mkdir(IOM *iom,INODE *parent_inode, char *name){

    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sbi = IOM_SB_I(iom);
    DIRENTRY *dir_new_entry, *dir_default_entry ,*tmp_dir_entry, dir_entry;
    INODE *new_inode;
	long blocknr_inbmap=0, read_bytes=0, parent_dir_size=0;
    char *buf_new_dir_area, *buf_parent_dir_area;   
    int cluster_size = sbi->cluster_size, parent_dir_blocknr;
	int res = LAB4_ERROR, isdelete = 0;

    res = lab4_lookup(iom, parent_inode->i_ino, &dir_entry,name,&isdelete);
    if(res == LAB4_SUCCESS)
        return -EEXIST;

    new_inode = alloc_new_inode(iom);
    if(new_inode == NULL){
        restore_ibmap(iom);
        return LAB4_ERROR;
    }
 
    res = alloc_block_inbmap(iom,&blocknr_inbmap);
    if(res == LAB4_ERROR){
        restore_dbmap(iom);
        return LAB4_ERROR;
    }
  
    /* directory block */
    buf_new_dir_area = (char*)malloc(sbi->cluster_size);
    if(!buf_new_dir_area)
        return LAB4_ERROR;

    memset(buf_new_dir_area, 0x0, sbi->cluster_size);
    
    dir_default_entry = (struct lab4_dir_entry*)buf_new_dir_area;

	strcpy(dir_default_entry[0].d_filename,".");
	dir_default_entry[0].d_ino = new_inode->i_ino;
	dir_default_entry[0].d_flag = LAB4_DIR_USED;

	strcpy((char*)dir_default_entry[1].d_filename,"..");
	dir_default_entry[1].d_ino = parent_inode->i_ino;
	dir_default_entry[1].d_flag = LAB4_DIR_USED;    

    sb->sb_last_allocated_blknr = blocknr_inbmap;

    /* directory inode */
    new_inode->i_type = LAB4_TYPE_DIRECTORY;
    new_inode->i_size = LAB4_DIR_ENTRY_SIZE * LAB4_DIR_ENTRY_NUM;		
	new_inode->i_version = 1;
	new_inode->i_ptr = 1;
	new_inode->i_gid = getgid();
	new_inode->i_uid = getuid();
	new_inode->i_mode = 0755 | S_IFDIR;
	new_inode->i_atime = time(NULL);
	new_inode->i_ctime = time(NULL);
	new_inode->i_mtime = time(NULL);
	new_inode->i_links_count = 2;
	new_inode->i_blocks[0] = sb->sb_darea_start*cluster_size + blocknr_inbmap*cluster_size;

	parent_dir_size = parent_inode->i_size;
    parent_dir_blocknr = parent_inode->i_blocks[0];

    dir_new_entry = (DIRENTRY*)malloc(LAB4_DIR_ENTRY_SIZE);
    if(!dir_new_entry)
        goto LAB4_FREE1;
    memset(dir_new_entry, 0x0, LAB4_DIR_ENTRY_SIZE);

	strcpy(dir_new_entry->d_filename,name);
	dir_new_entry->d_ino = new_inode->i_ino;
	dir_new_entry->d_flag = LAB4_DIR_USED;

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
    pwrite(iom->dev_fd, buf_new_dir_area,cluster_size,  blocknr_inbmap * cluster_size + sbi->darea_start);

    inc_link(iom,parent_inode->i_ino);
    update_sb(iom);    
    update_ibmap(iom);
    update_dbmap(iom);
    update_itble(iom);

    free(buf_parent_dir_area);
    free(buf_new_dir_area);
    return LAB4_SUCCESS;

LAB4_FREE2:
    free(buf_parent_dir_area);

LAB4_FREE1:
    free(new_inode);
    free(buf_new_dir_area);

    return LAB4_ERROR;
}

int check_dir_deletable(IOM *iom,INODE *del_inode){

    struct lab4_sb_info *sbi = IOM_SB_I(iom);
	struct lab4_dir_entry *tmp_dir;
	long read_bytes = 0;
	long dir_size = 0;
	int start = 0,res = -1,blocknr, count;
    char *buf_dir_entry;
 
    buf_dir_entry = (char*)malloc(iom->cluster_size);
    if(!buf_dir_entry)
        return LAB4_ERROR;
    memset(buf_dir_entry, 0x0, iom->cluster_size);

	dir_size = del_inode->i_size;
	start = 2*LAB4_DIR_ENTRY_SIZE;
    blocknr = del_inode->i_blocks[0];

    res = pread(iom->dev_fd,buf_dir_entry, iom->cluster_size, blocknr);
    if(res ==  LAB4_ERROR)
        goto FREE;
    tmp_dir = (struct lab4_dir_entry *)buf_dir_entry;

    /*FIXME - indirect block, triple block support, no such file */
	for (read_bytes = start,count=2; read_bytes < dir_size; read_bytes += LAB4_DIR_ENTRY_SIZE, count++) {

		if (tmp_dir[count].d_flag == LAB4_DIR_USED) {
             goto NOTEMPTY;
		}
	}
    return LAB4_SUCCESS;
NOTEMPTY:
    free(buf_dir_entry);
    return -ENOTEMPTY; 

FREE:
    free(buf_dir_entry);
    return LAB4_ERROR;
}

int do_rmdir(IOM *iom,INODE *parent_inode, char *filename){

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
        goto ERROR1;

    res = check_dir_deletable(iom,del_inode);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    else if(res == -ENOTEMPTY)
        return -ENOTEMPTY;

    res = delete_inode(iom, del_inode);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
   
    delete_block_inbmap(iom, del_inode);

    buf_zero = (char*)malloc(cluster_size);
    memset(buf_zero,0x0, cluster_size);
    
    buf_backup = (char*)malloc(cluster_size);
    memset(buf_backup, 0x0, cluster_size);

    res = pread(iom->dev_fd, buf_backup,cluster_size, del_inode->i_blocks[0]);
    if(res == LAB4_ERROR)
        goto ERROR2;
    
    res = pwrite(iom->dev_fd, buf_zero, cluster_size, del_inode->i_blocks[0]);
    if(res == LAB4_ERROR)
        goto ERROR3;

    if(isdelete !=-1){
        pwrite(iom->dev_fd, &del_entry,LAB4_DIR_ENTRY_SIZE,parent_inode->i_blocks[0]+isdelete);
    }
    dec_link(iom,parent_inode->i_ino);

    update_sb(iom);
    update_ibmap(iom);
    update_dbmap(iom);
    update_itble(iom);

    return LAB4_SUCCESS;

ERROR3:
    restore_data(iom,buf_backup, cluster_size, del_inode->i_blocks[0]);
ERROR2:
    restore_dbmap(iom);
ERROR1:
    restore_ibmap(iom);
    restore_itble(iom);
    return LAB4_ERROR;
}
