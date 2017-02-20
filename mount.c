/*
*	Operating System Lab
*	    Lab4 (File System in Userspace)
*	    Copyright (C) 2017 Juhyung Son <tooson9010@gmail.com>
*	    First Writing: 30/12/2016
*
*   mount.c :
*       - the main source to make lab4_mount executable file.
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
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/file.h>

#include "include/lab4_fs_types.h"

void lab4_fuse_mount_usage(char *cmd)
{
    printf("DKU Operating System Lab4 - fuse file system \n");
	printf("\n Usage for %s : \n",cmd);
	printf("    -argv[1] : device name to mount (e.g. /dev/sdc /dev/name0n1 )\n");
	printf("    -argv[2] : mount point (e.g. /mnt /home/son )\n\n");
}

void lab4_fuse_mount_example(char *cmd)
{
    printf("\n Example : \n");
    printf("    #sudo %s /dev/sdc /mnt \n", cmd);
    printf("    #sudo %s /dev/nvme0n1 /home/son \n\n", cmd);
}


#define LAB4_FORMAT_UNKNOWN LAB4_FORMAT_MISSING

IOM* check_formated(s8 *dev, s8* mnt){

    IOM *iom = NULL;
    struct lab4_super_block *sb;
    s8* buf_path, *buf_sb;
    s8 *buf_ibmap, *buf_dbmap, *buf_itble;
    s32 fid, cluster_size;

    s8 *dev_path, *mnt_path;

    iom = (IOM*)malloc(sizeof(IOM));
    if(!iom)
        return NULL;
    memset(iom, 0x0,sizeof(IOM));

    dev_path = (char*)malloc(strlen(dev)+1);
    memset(dev_path, 0x0, strlen(dev)+1);
    sprintf(dev_path, "%s", dev);
    strcpy(dev_path, dev); 

    mnt_path = (char*)malloc(strlen(mnt)+1);
    memset(mnt_path, 0x0, strlen(mnt)+1);
    strcpy(mnt_path, mnt);

    fid = open(dev_path, O_RDWR);
    if(fid == LAB4_ERROR){
        printf("can't open device %s \n",dev_path);
        return NULL;
    }

    iom->dev_fd = fid;
    iom->dev_path = dev_path;
    iom->mnt_path = mnt_path;

    fill_io_manager(iom, LAB4_FORMAT_UNKNOWN);

    if(iom){
        cluster_size = iom->cluster_size;
        buf_sb = (s8*)malloc(cluster_size);
        if(!buf_sb){
            printf("    malloc error \n");
            goto ERROR;
        }

        lseek(fid,RPOS_SB * cluster_size, SEEK_SET);
        printf("\n---------------------  format inspection   --------------------\n");

        printf("read super block for checking ... \n");
        if(read(fid, buf_sb, cluster_size) == LAB4_ERROR)
            goto ERROR;

        printf("inspecting if signature value is match ...\n");

        sb = (struct lab4_super_block*)buf_sb;
        iom->sb = sb;
                
        printf("    signature inspection is computed. \n\n");
        if(sb->sb_signature == LAB4_SB_SIGNATURE){

            printf("     - file system signature :   %x \n", sb->sb_signature);
            printf("     - OS Lab student id     :   %x \n\n", sb->sb_student_id);
            printf("    current %s is formated to lab \n", iom->dev_path);
            printf("    you can mount this device to mount point  \n");

            buf_ibmap = (s8*)malloc(cluster_size);
            lseek(fid,RPOS_IBMAP * cluster_size, SEEK_SET);
            read(fid, buf_ibmap, cluster_size);
            iom->iom_buf_ibmap = buf_ibmap;

            buf_dbmap = (s8*)malloc(cluster_size);
            lseek(fid,RPOS_DBMAP * cluster_size, SEEK_SET);
            read(fid, buf_dbmap, cluster_size);
            iom->iom_buf_dbmap = buf_dbmap;

            buf_itble = (s8*)malloc(cluster_size);
            lseek(fid,RPOS_ITBLE * cluster_size, SEEK_SET);
            read(fid, buf_itble, cluster_size);
            iom->iom_buf_itble = buf_itble;

        }else{
            printf("    signature missmatch. current signature value is %x  \n", sb->sb_signature);
            printf("    you can't mount this device to mount point  \n");

            goto ERROR;
        }
    }else{
        printf("    setting io manager error \n");
        return 0;
    }
	printf("\n--------------------- inspection successed --------------------\n");

    return iom;   
ERROR:
    printf("    mount failed  \n");
	printf("\n---------------------  inspection failed   --------------------\n");

    free_buf(buf_sb);

    return NULL;
}

int lab4_fill_super(IOM *iom){

    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_sb_info *sb_i = NULL;
    int cluster_size;

    sb_i = (struct lab4_sb_info*)malloc(sizeof(struct lab4_sb_info));
    sb_i->student_id = sb->sb_student_id;
    sb_i->sector_size = sb->sb_sector_size;
    sb_i->cluster_size = sb->sb_sector_size << sb->sb_block_size_shift;
    cluster_size = sb_i->cluster_size;

    sb_i->no_of_sectors = sb->sb_no_of_sectors;
    sb_i->no_of_used_blocks = sb->sb_no_of_used_blocks;
    sb_i->root_ino = sb->sb_root_ino;
    sb_i->reserved_ino = sb->sb_reserved_ino;

    sb_i->reserved = sb->sb_reserved;		
    sb_i->sb_start = sb->sb_sb_start * cluster_size;
    sb_i->sb_size = sb->sb_sb_size * cluster_size;
	sb_i->ibitmap_start = sb->sb_ibitmap_start * cluster_size;
	sb_i->ibitmap_size = sb->sb_ibitmap_size * cluster_size;
	sb_i->dbitmap_start = sb->sb_dbitmap_start * cluster_size;
	sb_i->dbitmap_size = sb->sb_dbitmap_size * cluster_size;
	sb_i->itable_start = sb->sb_itable_start * cluster_size;
	sb_i->itable_size = sb->sb_itable_size * cluster_size;
	sb_i->darea_start = sb->sb_darea_start * cluster_size;
	sb_i->darea_size = sb->sb_darea_size * cluster_size;

    sb_i->inode_size = sb->sb_inode_size;
    sb_i->direntry_size = sb->sb_direntry_size;
    sb_i->last_allocated_ino = sb->sb_last_allocated_ino;
    sb_i->last_allocated_blknr = sb->sb_last_allocated_blknr;

    sb_i->file_table = (struct lab4_file_table *)malloc(sizeof(struct lab4_file_table) * MAX_OPEN_FILE);
    if (sb_i->file_table == NULL) {
        return LAB4_ERROR;
    }
    memset(sb_i->file_table, 0x00, sizeof(struct lab4_file_table) * MAX_OPEN_FILE);


    iom->sbi = sb_i;

    return LAB4_SUCCESS;
}



