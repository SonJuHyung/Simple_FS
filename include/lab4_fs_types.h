/*
*	LAB4 (NVMe based File System in Userspace)
*	Copyright (C) 2016 Yongseok Oh <yongseok.oh@sk.com>
*	First Writing: 30/10/2016
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

#include "lab4_config.h"
#include "lab4_types.h"
//#include "LAB4_io_manager.h"
//#include "LAB4_bp_tree.h"
#include "lab4_list.h"
#include "lab4_rbtree.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string.h>
#include <fuse.h>

#ifndef _LAB4_HEADER_H
#define _LAB4_HEADER_H


/* LAB4 SPECIAL SIGNATURE */
#define LAB4_SB_SIGNATURE	    0x3442414C
#define LAB4_SB_STD_ID	        0x60181132

/* INITIAL BLOCK NUMBER */
#define INIT_LAB4_SUPERBLOCK_NO		0

/* INODE NUMBER */
#define LAB4_RESRV0_INO		0
#define LAB4_RESRV1_INO		1
#define LAB4_ROOT_INO		2 /* root directory */
#define LAB4_NUM_RESV_INO   3

#define LAB4_TYPE_UNKOWN	1
#define LAB4_TYPE_SPECIAL	2
#define LAB4_TYPE_INODE		3
#define LAB4_TYPE_FILE		4
#define LAB4_TYPE_INDIRECT	5
#define LAB4_TYPE_DIRECTORY	6
#define LAB4_TYPE_TIME		7
#define LAB4_TYPE_BPTREE	8

/* DIR RELATED */
#define LAB4_SB_SIZE    sizeof(struct lab4_super_block)
#define LAB4_DIR_ENTRY_SIZE sizeof(struct lab4_dir_entry)
#define LAB4_DIR_ENTRY_NUM (CLUSTER_SIZE/LAB4_DIR_ENTRY_SIZE)
#define FNAME_SIZE (116)

#define LAB4_DIR_EMPTY	(0)
#define LAB4_DIR_USED	(1 << 1)
#define LAB4_DIR_DELETED (1 << 2)

#define LAB4_INODE_ENTRY_SIZE sizeof(struct lab4_inode)
#define LAB4_INODE_ENTRY_NUM	(CLUSTER_SIZE/LAB4_INODE_ENTRY_SIZE)

#define IFILE_ENTRY_SIZE INODE_ENTRY_SIZE
#define IFILE_ENTRY_NUM	INODE_ENTRY_NUM


/* Various Macro Definition */
#define LAB4_CLUSTER_SIZE (CLUSTER_SIZE)
#define LAB4_CLUSTER_SIZE_BITS CLUSTER_SIZE_BITS
#define LAB4_SIZE_TO_BLK(x) ((s32)((s64)(x) >> LAB4_CLUSTER_SIZE_BITS))
#define LAB4_CLU_P_SEG(sb) (sb->sb_no_of_blocks_per_seg)
#define LAB4_CLUSTER_PER_SEGMENTS_BITS(s_bits,c_bits) (s_bits - c_bits)
#define LAB4_CLU_P_SEG_BITS(sb) LAB4_CLUSTER_PER_SEGMENTS_BITS(sb->sb_segment_size_bits, CLUSTER_SIZE_BITS)
#define LAB4_SEG_NUM(num_clu, clu_per_seg_bits) (num_clu >> clu_per_seg_bits)
#define LAB4_GET_SID(sb,clu) (clu >> LAB4_CLU_P_SEG_BITS(sb))
#define LAB4_GET_SEG_TO_CLU(sb,seg) (seg << LAB4_CLU_P_SEG_BITS(sb))
#define LAB4_NUM_CLU (DISK_SIZE >> CLUSTER_SIZE_BITS)


/* # OF MAX OPEN FILE */
#define START_OPEN_FILE	3 /* STDIN, STDOUT, STDERR*/
#define MAX_OPEN_FILE	16

#define LAB4_BP_TYPE_DATA 0
#define LAB4_BP_TYPE_DIR 1
#define LAB4_BP_TYPE_ETC 2

#define LAB4_BP_TYPE_BITS 2
#define LAB4_BP_HIGH_BITS 32
#define LAB4_BP_LOW_BITS 32
#define LAB4_BP_COLLISION_BITS 2

#define LAB4_MAX_BITS 32
#define LAB4_MAX_INODE_BITS 30
#define LAB4_MAX_DIR_BITS 32
#define LAB4_MAX_FILE_BITS 32

#define LAB4_BLK_SIZE_SHIFT 3

struct lab4_super_block{
	u32 sb_signature; //RDONLY	
    u32 sb_student_id;

	s64	sb_sector_size;//RDONLY	
    s64 sb_block_size_shift;
	s64	sb_no_of_sectors;//RDONLY	
	s64	sb_no_of_used_blocks;

	u32	sb_root_ino;/* RDONLY */
    u32 sb_reserved_ino;

	s32	sb_max_file_num;
	s32	sb_max_dir_num;
	s32	sb_max_inode_num;

    u32 sb_reserved;
    u32 sb_sb_start;
    u32 sb_sb_size;
	u32 sb_ibitmap_start;
	u32 sb_ibitmap_size;
	u32 sb_dbitmap_start;
	u32 sb_dbitmap_size;
	u32 sb_itable_start;
	u32 sb_itable_size;
	u32 sb_darea_start;
	u32 sb_darea_size;

	s32 sb_free_inodes;
	s64 sb_free_blocks;

    u32 sb_umount;

    u32 sb_inode_size;
    u32 sb_direntry_size;
    u32 sb_last_allocated_ino;
    u32 sb_last_allocated_blknr;
};

#define MAX_FILES_PER_DIR (0x7FFFFFFF)

struct lab4_dir_entry{	
	inode_t	d_ino;
	u32	d_flag;
	u32	d_version;
	s8	d_filename[FNAME_SIZE];	
};

#define LAB4_SUPERBLOCK_OFFSET  0
#define LAB4_SUPERBLOCK_SIZE    1
#define LAB4_IBITMAP_OFFSET     1
#define LAB4_IBITMAP_SIZE       1
#define LAB4_DBITMAP_OFFSET     (LAB4_IBITMAP_OFFSET+LAB4_IBITMAP_SIZE) // 2
#define LAB4_DBITMAP_SIZE       1
#define LAB4_ITABLE_OFFSET      (LAB4_DBITMAP_OFFSET+LAB4_DBITMAP_SIZE) // 3
#define LAB4_ITABLE_SIZE        1
#define LAB4_DAREA_OFFSET       (LAB4_ITABLE_OFFSET + LAB4_ITABLE_SIZE) // 4
#define LAB4_RESERVED           LAB4_DAREA_OFFSET // 4

#define TYPE_DIRECT         0
#define TYPE_INDIRECT       TYPE_DIRECT+1
#define TYPE_DINDIRECT      TYPE_INDIRECT+1
#define TYPE_TINDIRECT      TYPE_DINDIRECT+1

#define DIRECT_BLOCKS       11
#define INDIRECT_BLOCKS     12
#define DINDIRECT_BLOCKS    13
#define TINDIRECT_BLOCKS    14
#define INDIRECT_BLOCKS_LEVEL 4

#define PTRS_PER_BLOCK		(CLUSTER_SIZE/sizeof(u32))
#define PTRS_PER_BLOCK_BITS	10

#define MAX_FILE_SIZE ((s64)DIRECT_BLOCKS * CLUSTER_SIZE + \
						(s64)(1 << PTRS_PER_BLOCK_BITS) * CLUSTER_SIZE + \
						(s64)(1 << (PTRS_PER_BLOCK_BITS * 2)) * CLUSTER_SIZE + \
						(s64)(1 << (PTRS_PER_BLOCK_BITS * 3)) * CLUSTER_SIZE)

struct lab4_inode{	
	inode_t	i_ino;                                      //
	u32	i_type;                                         //
	s64	i_size;	                                        //
	u32	i_version;                                      //
	u32	i_deleted;                                      //
	u32	i_links_count;	/* Links count */               //
	u32	i_ptr;		    /* for directory entry */       //
	u32	i_atime;	    /* Access time */               //
	u32	i_ctime;	    /* Inode change time */         //
	u32	i_mtime;	    /* Modification time */         //
	u32	i_dtime;	    /* Deletion Time */             //
	u16	i_gid;		    /* Low 16 bits of Group Id */   //
	u16	i_uid;		    /* Low 16 bits of Owner Uid */	//
	u16	i_mode;		    /* File mode */                 //
	u32 resv1[1];                                       //
	u32 i_blocks[TINDIRECT_BLOCKS + 1];                 //
	u32 resv2[1];	                                    //
    /*FIXME*/
    //u32 i_flags;
};


/*
 * lab4 file system usage context
 */
#define CURRENT_FORMAT(TYPE) \
    (TYPE == LAB4_FORMAT_LAB) ? printf("lab") : printf("fat") 

#define IOM_SB(IOM) {IOM->sb}
#define IOM_SB_I(IOM) {IOM->sbi}
#define IOM struct io_manager
#define SIZE(TYPE) sizeof(struct TYPE)
#define INODE struct lab4_inode
#define DIRENTRY struct lab4_dir_entry

struct io_manager{
    s8 *dev_path;
    s8* mnt_path;
    int dev_fd;
    int no_of_sectors;
    int blk_size;
    int cluster_size;
    int format_type;
    struct lab4_super_block *sb;
    struct lab4_sb_info *sbi;
    char *iom_buf_ibmap;
    char *iom_buf_dbmap;
    char *iom_buf_itble;
};

#define RPOS_SB     0
#define RPOS_IBMAP  1
#define RPOS_DBMAP  2
#define RPOS_ITBLE  3
#define RPOS_DAREA  4
#define RPOS_RESV   5

struct reserved_pos{
    s8 *buf;
    int pos;
};

/*
 * lab4 file system in memory structure
 */
typedef s64 lab4_off_t;
typedef u32	lab4_loff_t;

struct lab4_file_table{	
	inode_t	ino;
	s64	size;
	s32	used;
	lab4_off_t prefetch_cur;
	lab4_off_t rwoffset;
	s32 flags;
//	pthread_mutex_t ft_lock;
};

struct lab4_sb_info {
    unsigned int student_id;

	long    sector_size;//RDONLY	
    long    cluster_size;
	long	no_of_sectors;//RDONLY	
	long	no_of_used_blocks;

	unsigned int  root_ino;/* RDONLY */
    unsigned int  reserved_ino;

    unsigned int reserved;
	unsigned int max_inodes;
	unsigned int max_blocks;
    unsigned int sb_start;
    unsigned int sb_size;
	unsigned int ibitmap_start;
	unsigned int ibitmap_size;
	unsigned int dbitmap_start;
	unsigned int dbitmap_size;
	unsigned int itable_start;
	unsigned int itable_size;
	unsigned int darea_start;
	unsigned int darea_size;

	int sb_free_inodes;
	long sb_free_blocks;
    unsigned int inode_size;
    unsigned int direntry_size;
    unsigned int last_allocated_ino;
    unsigned int last_allocated_blknr;

    struct lab4_file_table *file_table;
};


struct lab4_inode_info{
 	unsigned int i_data[15];
    //u32 i_flags;
    unsigned int i_faddr;
	unsigned int i_state;
    /*
	unsigned int i_file_acl;
	unsigned int i_dir_acl;
    */
	unsigned int i_dtime;

	unsigned int i_dir_start_lookup;

	struct list_head i_orphan;	/* unlinked but open inodes */
     
};

/* fsformat.c */
IOM* set_io_manager(char *path, s32 flags);
u64 get_no_of_sectors(struct io_manager *iom);
s32 check_flag(s32 fid, s32 *state_flag,s32 *access_flag);
s32 close_io_manager(struct io_manager *iom);
s32 fill_io_manager(struct io_manager *iom,int format_type);
s32 format_device(struct io_manager *iom);
s32 format_device_to_lab(struct io_manager *iom);
s32 format_device_to_fat(struct io_manager *iom);
s32 free_buf(s8 *buf);
s32 lab4_alloc_root_inode_direct(struct reserved_pos *r_pos, struct io_manager *iom);
s32 print_flag(s32 fid);
s32 zeroing_device(struct io_manager *iom);
void lab4_fuse_mkfs_example(char *cmd);
void lab4_fuse_mkfs_usage(char *cmd);
void print_io_manager(struct io_manager *iom);

/* mount.c */
IOM* check_formated(s8 *dev_path, s8* mnt_path);
int lab4_fill_super(IOM *iom);
void lab4_fuse_mount_example(char *cmd);
void lab4_fuse_mount_usage(char *cmd);

/* inode.c */
int do_path_check(const char *path);
int do_fill_inode(IOM *iom, struct lab4_inode *inode, struct stat *stbuf);
int lab4_read_inode(IOM *iom, const char *path, INODE **inode);
INODE* do_read_inode(IOM *iom, inode_t ino);
inode_t get_root_ino(struct lab4_sb_info *sbi);
INODE* alloc_new_inode(IOM *iom);
int update_sb(IOM *iom);
int update_ibmap(IOM *iom);
int update_dbmap(IOM *iom);
int update_itble(IOM *iom);
int restore_sb(IOM *iom);
int restore_ibmap(IOM *iom);
int restore_dbmap(IOM *iom);
int restore_itble(IOM *iom);
int restore_data(IOM *iom,char *buf, int size, long blocknr);
int delete_inode(IOM *iom, INODE *del_inode);
void delete_block_inbmap(IOM *iom, INODE * blocknr);
void inc_link(IOM *iom, inode_t par_ino);
void dec_link(IOM *iom, inode_t par_ino);

void inc_free_inodes(IOM *iom);
void dec_free_inodes(IOM *iom);
void inc_free_blocks(IOM *iom);
void dec_free_blocks(IOM *iom);

int do_utimens(IOM *iom, const char *path, const struct timespec ts[2]);


/* dir.c */
int do_readdir(IOM *iom,INODE* dir_inode, int dir_offset,void* buf, fuse_fill_dir_t filler);
int do_mkdir(IOM *iom,INODE *inode, char *name);
int do_rmdir(IOM *iom,INODE *dir_inode, char *name);
int check_dir_deletable(IOM *iom,INODE *del_inode);
int get_name_from_path(const char *path, char *f_name, char* d_name);
int lab4_lookup(IOM *iom, inode_t parent_dir_ino, struct lab4_dir_entry *file_entry, const char *filename,int *isdelete);
/* disk_map.c */

/* file.c */
int lab4_allocate_open_file_table(IOM *iom);
int lab4_do_open(IOM *iom, const char *path, int flags, int mode);
int lab4_createfile(IOM *iom, INODE *parent_inode,char *filename, INODE **new_ino, mode_t mode);
int lab4_seek(struct lab4_super_block *sb, struct lab4_file_table *ft,long offset, int position);
void lab4_do_close(IOM *iom, int fid);
int do_unlink(IOM *iom, INODE *parent_inode, char *filename);


/* bmap.c  */
int alloc_block_inbmap(IOM *iom, long *blocknr);
int scan_allocate_ibmap(IOM *iom, inode_t last_allocated_ino, inode_t *new_ino);
int scan_allocate_dbmap(IOM *iom, long *blocknr);
void scan_delete_ibmap(IOM *iom, inode_t del_ino);
void scan_delete_dbmap(IOM *iom, int blocknr_inbmap);
int write_inode_to_itble(IOM *iom, INODE *new_inode);

/* bmap_raw.c  */
s32 lab4_clear_bit(u32 nr, void * addr);
s32 lab4_set_bit(u32 nr,void * addr);
s32 lab4_test_bit(u32 nr, const void * addr);

#endif /* LAB4_HEADER_H*/
