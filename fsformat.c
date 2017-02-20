/*
*	Operating System Lab
*	    Lab4 (File System in Userspace)
*	    Copyright (C) 2017 Juhyung Son <tooson9010@gmail.com>
*	    First Writing: 30/12/2016
*
*   fsformat.c :
*       - the main source file for the file system formatting utility.
*       - format device to user defined super block,inode, directory entries.
*       - make ondisk layout of fuse file system. 
*       - ondisk layout.
*         |------------|---------|---------|-------------|-------------|
*         | super block| ibitmap | dbitmap | inode table | data blocks |
*         |------------|---------|---------|-------------|-------------|
*           + super block ( lab4_super_block )
*           + inode       ( lab4_inode ) 
*           + direntry    ( lab4_dir_entry )
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


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/ioctl.h>

#include "include/lab4_types.h"
#include "include/lab4_fs_types.h"
#include "include/lab4_timeval.h"

void lab4_fuse_mkfs_usage(char *cmd)
{
	printf("\n Usage for %s : \n",cmd);
	printf("    -d: device name to format (e.g. /dev/sdc1 /dev/name0n1 )\n");
	printf("    -f: format type (e.g. fat, lab)\n\n");
}

void lab4_fuse_mkfs_example(char *cmd)
{
    printf("\n Example : \n");
    printf("    #sudo %s -d /dev/sdc1 -f fat \n", cmd);
    printf("    #sudo %s -d /dev/nvme0n1 -f lab \n\n", cmd);
}

u64 get_no_of_sectors(IOM *iom)
{
	u64 blk_size=0,no_of_sectors=0;

    if(!iom)
        return LAB4_INVALID_ARGS;
    

	ioctl(iom->dev_fd, BLKGETSIZE, &no_of_sectors);
    ioctl(iom->dev_fd, BLKSSZGET, &blk_size);

    iom->no_of_sectors = no_of_sectors;
    iom->blk_size = blk_size;
    iom->cluster_size = iom->blk_size << LAB4_BLK_SIZE_SHIFT;

	return 0;
}

s32 check_flag(s32 fid,s32 *state_flag, s32 *access_flag){

    s32 cur_flag;

    cur_flag = fcntl(fid, F_GETFL,0);
    if(cur_flag == LAB4_ERROR)
        return LAB4_ERROR;

    *state_flag = cur_flag; 
    *access_flag = *state_flag & O_ACCMODE;

    return LAB4_SUCCESS;
}

s32 print_flag(s32 fid){
    s32 access_flag, stat_flags;

    if(check_flag(fid,&stat_flags, &access_flag) == LAB4_ERROR){
        printf("    error in print_flag - access check error \n");
        return LAB4_ERROR;
    }

    switch(access_flag){
        case O_RDONLY :
            printf("O_RDONLY");
            break;
        case O_WRONLY :
            printf("O_WRONLY");
            break;
        case O_RDWR :
            printf("O_RDWR");
            break;
    }
    
    if (stat_flags & O_CREAT) printf(" | O_CREAT");
    if (stat_flags & O_EXCL) printf(" | O_EXCL");
    if (stat_flags & O_TRUNC) printf(" | O_TRUNC");
    if (stat_flags & O_APPEND) printf(" | O_APPEND");
    if (stat_flags & O_ASYNC) printf(" | O_ANSYC");
    if ((stat_flags & O_NONBLOCK) == O_NONBLOCK)
        printf(" | O_NONBLOCK");
    else
        printf(" | BLOCKING");
    printf("\n");

    return LAB4_SUCCESS;
}

s32 fill_io_manager(IOM *iom,int format_type){

    s32 access_flag, state_flag;

    if(format_type != LAB4_FORMAT_MISSING)        
        iom->format_type = format_type;
    else
        iom->format_type = LAB4_FORMAT_MISSING;

    if(check_flag(iom->dev_fd, &state_flag, &access_flag) == LAB4_ERROR)
        return LAB4_ERROR;

    if(access_flag != O_WRONLY){
        if(get_no_of_sectors(iom) != LAB4_SUCCESS){
            printf("    error in fill_io_manager \n");
            return LAB4_ERROR;
        }
    }else{
        printf("    errpr in fill_io_manager - can't read %s device... \n \
                        (current flag is O_WRONLY)",iom->dev_path);
    }
    


    return LAB4_SUCCESS;
   
}

IOM* set_io_manager(char *path, s32 flags){
    
    int fd;
    IOM *iom;

    if(!path)
        return NULL;

    iom = (IOM*)malloc(sizeof(IOM));
    if(!iom){
        perror("    error in set_io_manager(malloc for iom) \n");
        return NULL;
    }
    memset(iom, 0, sizeof(IOM));

    fd = open(path, flags);
    if(fd < 0){
        perror("    error info : cannot open device \n");
        return NULL;
    }
       
    iom->dev_path = (s8*)path;
    iom->dev_fd = fd;

    return iom;
}

s32 close_io_manager(IOM *iom){
    if(iom){
        if(iom->dev_path){
            close(iom->dev_fd);
            free(iom->dev_path);
        }
        free(iom);
    }
    return LAB4_SUCCESS;
}

s32 format_device_to_fat(IOM *iom){
    if(iom)
        printf("fat type is not implemented ...\n");
    return LAB4_SUCCESS;
}

s32 lab4_alloc_root_inode_direct(struct reserved_pos *r_pos, IOM *iom){
    struct lab4_super_block *sb = IOM_SB(iom);
    struct lab4_inode *inode; 
    struct lab4_dir_entry *d_entry;
    s8 *buf_ibmap = r_pos[RPOS_IBMAP].buf;
    s8 *buf_dbmap = r_pos[RPOS_DBMAP].buf;
    s8 *buf_itble = r_pos[RPOS_ITBLE].buf;
    s8 *buf_darea = r_pos[RPOS_DAREA].buf;
    u32 ino = 0;

    for (ino = 0; ino < LAB4_NUM_RESV_INO; ino++)
    {
        lab4_set_bit(ino, buf_ibmap); // ibitmap 영역의 ino에 해당하는  bit를 set해줌. 
        sb->sb_free_inodes--;
    }
    
    /* FIXME */
	lab4_set_bit(0, buf_dbmap);

    inode = (struct lab4_inode *)buf_itble;
	for (ino = 0; ino < LAB4_NUM_RESV_INO; ino++)
	{
		inode[ino].i_ino = ino;
		if(ino != LAB4_ROOT_INO)
			continue;

		//root inode
		inode[ino].i_ino = LAB4_ROOT_INO;
		inode[ino].i_type = LAB4_TYPE_DIRECTORY;
		inode[ino].i_size = LAB4_DIR_ENTRY_SIZE * LAB4_DIR_ENTRY_NUM;		
		inode[ino].i_version = 1;
		inode[ino].i_ptr = 1;
		inode[ino].i_gid = getgid();
		inode[ino].i_uid = getuid();
		inode[ino].i_mode = 0755 | S_IFDIR;
		inode[ino].i_atime = time(NULL);
		inode[ino].i_ctime = time(NULL);
		inode[ino].i_mtime = time(NULL);
		inode[ino].i_links_count = 2;
		inode[ino].i_blocks[0] = sb->sb_darea_start * iom->cluster_size;
	}

    d_entry = (struct lab4_dir_entry*)buf_darea;

    /* root directoy dir_entry create  */
	strcpy((char*)d_entry[0].d_filename,".");
	d_entry[0].d_ino = LAB4_ROOT_INO;
	d_entry[0].d_flag = LAB4_DIR_USED;

	strcpy((char*)d_entry[1].d_filename,"..");
	d_entry[1].d_ino = LAB4_ROOT_INO;
	d_entry[1].d_flag = LAB4_DIR_USED;    

    return LAB4_SUCCESS;
}

s32 free_buf(s8 *buf){
    if(buf != NULL)
        free(buf);
    return LAB4_SUCCESS;
}

/*FIXME*/
s32 zeroing_device(IOM *iom){
    u64 cur_pos;
    u32 blk_size = blk_size;
    u32 max_size = blk_size * iom->no_of_sectors;
    u32 cluster_size = iom->cluster_size;
    u32 fid = iom->dev_fd;
    s64 res = LAB4_SUCCESS;
    s8 *buf = (s8*)malloc(sizeof(s8)*cluster_size);
    if(!buf){
        printf("zeroing_device error(allc error) \n");
        res = LAB4_ERROR;
        goto OUT;
    }
    memset(buf, 0x00, cluster_size);

    for(cur_pos = 0 ; cur_pos < max_size ; cur_pos += cluster_size){
        lseek(fid, cur_pos,SEEK_SET);
        res = write(fid,buf,cluster_size);
        if(res == LAB4_ERROR){
            printf("zeroing_device error(zeroing error) \n");
            res = LAB4_ERROR;
            goto OUT;
        }
        printf("\r      written byte : %.3Lf MB", ((long double)res / 1024.0 / 1024.0));
    }
    lseek(fid,0,SEEK_SET);

OUT:
    free_buf(buf);
    return LAB4_SUCCESS;
}

void print_io_manager(IOM *iom){
    
    printf("target device %s is \n ",iom->dev_path);
    printf("    fid             : %u \n", iom->dev_fd);
	printf("    sectors         : %u \n", iom->no_of_sectors);
	printf("    block size      : %u \n", iom->blk_size);
    printf("    cluster size    : %u \n", iom->cluster_size);

    printf("    format type     : ");
    CURRENT_FORMAT(iom->format_type);
    printf("\n\n");
}


s32 format_device_to_lab(IOM *iom){

	s8 *buf_sb, *buf_ibmap, *buf_dbmap, *buf_itble, *buf_darea;
    s32 pos_sb, pos_ibmap, pos_dbmap, pos_itble, pos_darea;
    s32 blk_size = iom->blk_size;
    s32 cluster_size = iom->cluster_size;
    s32 no_of_sectors = iom->no_of_sectors;
    s32 dev_fd = iom->dev_fd;
    s32 res;
    struct timeval tv,tv_end;

	struct lab4_super_block	*sb;
    struct reserved_pos r_pos[RPOS_RESV];
    /* temp for test */
    struct lab4_inode inode_test;
    struct lab4_super_block sb_test;
    struct lab4_dir_entry de_test;
    /* temp for test */

	printf("-------------------------------------------------------------------\n");
    printf("Formatting target device to lab file system \n ");
	printf(" Warning: your data will be removed permanentlyi ...\n");
    printf("    super block size    : %ld \n", LAB4_SB_SIZE);
    printf("    inode size          : %ld \n", LAB4_INODE_ENTRY_SIZE);
    printf("    dir entry size      : %ld \n", LAB4_DIR_ENTRY_SIZE);
	printf("\n\n");
	gettimeofday(&tv, NULL);

    /*
    if(zeroing_device(iom) == LAB4_ERROR){
        printf("zering format error !!! ");
        printf("\n lab4 fuse file system format failed. \n");
        printf("-------------------------------------------------------------------\n");
    }
    */

    /* alloc sb  */
    buf_sb = (s8*)malloc(cluster_size);
	memset(buf_sb, 0x00, cluster_size);
    /* Initialize sb */
	sb = (struct lab4_super_block *) buf_sb;
    if(!sb){
        perror("super block alloc error - ");
        goto OUT;
    }
    iom->sb = sb;

    /* alloc ibmap  */  
    buf_ibmap = (s8*)malloc(cluster_size);
	memset(buf_ibmap, 0x00, cluster_size);
    if(!buf_ibmap){
        perror("ibamp alloc error - ");
        goto OUT;
    }
    /* alloc dbmap  */
    buf_dbmap = (s8*)malloc(cluster_size);
	memset(buf_dbmap, 0x00, cluster_size);
    if(!buf_dbmap){
        perror("dbmap block alloc error - ");
        goto OUT;
    }
    /* alloc itble  */
    buf_itble = (s8*)malloc(cluster_size);
	memset(buf_itble, 0x00, cluster_size);
    if(!buf_itble){
        perror("itble alloc error - ");
        goto OUT;
    }

    /* alloc darea for init  */
    buf_darea = (s8*)malloc(cluster_size);
	memset(buf_darea, 0x00, cluster_size);
    if(!buf_darea){
        perror("darea alloc error - ");
        goto OUT;
    }

    /*Initialize sb*/
    sb->sb_signature = LAB4_SB_SIGNATURE;
    sb->sb_student_id = LAB4_SB_STD_ID;
    sb->sb_no_of_sectors = no_of_sectors;
    sb->sb_sector_size = blk_size;
    sb->sb_block_size_shift = LAB4_BLK_SIZE_SHIFT;

    sb->sb_reserved = LAB4_RESERVED;
    sb->sb_sb_start = LAB4_SUPERBLOCK_OFFSET;
    sb->sb_sb_size = LAB4_SUPERBLOCK_SIZE;
	sb->sb_ibitmap_start = LAB4_IBITMAP_OFFSET;
	sb->sb_ibitmap_size = LAB4_IBITMAP_SIZE; 

	sb->sb_dbitmap_start = LAB4_DBITMAP_OFFSET;
	sb->sb_dbitmap_size = LAB4_DBITMAP_SIZE; 
	sb->sb_itable_start = LAB4_ITABLE_OFFSET;
	sb->sb_itable_size = LAB4_ITABLE_SIZE;
	sb->sb_darea_start = LAB4_DAREA_OFFSET;
	sb->sb_darea_size = (sb->sb_no_of_sectors)/cluster_size - sb->sb_dbitmap_start; 
	sb->sb_darea_size = (sb->sb_no_of_sectors * sb->sb_sector_size) / cluster_size - LAB4_RESERVED; 

	sb->sb_free_inodes = (LAB4_ITABLE_SIZE * cluster_size) / LAB4_INODE_ENTRY_SIZE;
	sb->sb_free_blocks = sb->sb_darea_size;

    sb->sb_inode_size = LAB4_INODE_ENTRY_SIZE;
    sb->sb_direntry_size = LAB4_DIR_ENTRY_SIZE;
    sb->sb_last_allocated_ino = LAB4_ROOT_INO+1;
    sb->sb_last_allocated_blknr = 1;

    pos_sb = LAB4_SUPERBLOCK_OFFSET * cluster_size;
    pos_ibmap = LAB4_IBITMAP_OFFSET * cluster_size;
    pos_dbmap = LAB4_DBITMAP_OFFSET * cluster_size;
    pos_itble = LAB4_ITABLE_OFFSET * cluster_size;
    pos_darea = LAB4_DAREA_OFFSET * cluster_size;

    /* make struct for the sake of parameters  */
    r_pos[RPOS_SB].buf = buf_sb;
    r_pos[RPOS_SB].pos = pos_sb;
    
    r_pos[RPOS_IBMAP].buf = buf_ibmap;
    r_pos[RPOS_IBMAP].pos = pos_ibmap;

    r_pos[RPOS_DBMAP].buf = buf_dbmap;
    r_pos[RPOS_DBMAP].pos = pos_dbmap;
    
    r_pos[RPOS_ITBLE].buf = buf_itble;
    r_pos[RPOS_ITBLE].pos = pos_itble;

    r_pos[RPOS_DAREA].buf = buf_darea;
    r_pos[RPOS_DAREA].pos = pos_darea;

    sb->sb_reserved_ino = LAB4_NUM_RESV_INO;
    sb->sb_root_ino = LAB4_ROOT_INO;

    lab4_alloc_root_inode_direct(r_pos, iom);

    /*
	memset(inode, 0x00, LAB4_INODE_ENTRY_SIZE*5);
	root_inode = &inode[0];
	ifile_inode = &inode[1];
	su_inode = &inode[2];
	bp_tree_inode = &inode[3];
	ss_inode = &inode[4];
	*/
    sb->sb_no_of_sectors = no_of_sectors;
    /*FIXME*/
	sb->sb_umount = 1;
    
    /* write sb  */
    lseek(dev_fd, 0, SEEK_SET);
    res = write(dev_fd,buf_sb, cluster_size);
    printf("    sb area size        : %d B \n", res);
    printf("    sb area pos         : %x \n\n", pos_sb);

    /* write ibmap  */
    lseek(dev_fd, pos_ibmap, SEEK_SET); 
    res = write(dev_fd,buf_ibmap, cluster_size);
    printf("    ibmap area size     : %d B \n", res);
    printf("    ibmap area pos      : %x \n\n", pos_ibmap);


    /* write dmap  */
    lseek(dev_fd, pos_dbmap, SEEK_SET);   
    res = write(dev_fd,buf_dbmap, cluster_size);
    printf("    dbmap area size     : %d B \n", res);
    printf("    dbmap area pos      : %x \n\n", pos_dbmap);

    /* write itble  */
    lseek(dev_fd, pos_itble, SEEK_SET);
    res = write(dev_fd,buf_itble, cluster_size);
    printf("    itble area size     : %d B \n", res);
    printf("    itble area pos      : %x \n\n", pos_itble);

    lseek(dev_fd, pos_darea, SEEK_SET);
    res = write(dev_fd,buf_darea, cluster_size);
    printf("    data area size      : %d B \n", res);
    printf("    data area pos       : %x \n\n", pos_darea);

//    sync();
	fflush(stdout);

    gettimeofday(&tv_end,NULL);
		
    printf("\n lab4 fuse file system formatted successfully. (%.3f sec)\n",get_timeval(&tv,&tv_end));
	printf("-------------------------------------------------------------------\n");

OUT :  
    free_buf(buf_sb);
    free_buf(buf_ibmap);
    free_buf(buf_dbmap);
    free_buf(buf_itble);
    free_buf(buf_darea);

	return LAB4_SUCCESS;
    
}

s32 format_device(IOM *iom){
    if(iom->format_type == LAB4_FORMAT_LAB)
        return format_device_to_lab(iom);
    else if(iom->format_type == LAB4_FORMAT_FAT)
        return format_device_to_fat(iom);
    else{
        printf("format type error \n");
        return LAB4_ERROR;
    }
}

