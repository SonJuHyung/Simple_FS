/*
*	Operating System Lab
*	    Lab4 (File System in Userspace)
*	    Copyright (C) 2017 Juhyung Son <tooson9010@gmail.com>
*	    First Writing: 30/12/2016
*
*   fuse_main.c :
*       - check if the device is formated. 
*       - mount device to mount point.
*       - basic fuse operations.
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

#define FUSE_USE_VERSION 30

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



/* Debug Macro  */
#define LAB4_FUSE 1
#define EXAMPLE_FUSE 0
#define DEBUG 1

#if LAB4_FUSE 

static int test_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){

    char file54Text[] = "Hello World From File54!";
    char file349Text[] = "Hello World From File 349!";
    char *selectedText = NULL;

    if(strcmp(path, "/file54")==0)
        selectedText = file54Text;
    else if(strcmp(path, "/file349") == 0)
        selectedText = file349Text;
    else
        return -1;

    memcpy(buffer, selectedText + offset , size);
    return strlen(selectedText) - offset;


}

static int test_readdir(const char* path, void  *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){

    printf(" --> Getting The List of Files of %s\n", path);

    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);

    if(strcmp(path, "/") == 0){
        filler(buffer, "files54", NULL, 0);
        filler(buffer, "file349", NULL, 0);
    }

    return 0;
}

static int test_getattr(const char* path, struct stat *st){

    printf("[getattr] Called \n");
    printf("\tAttributes of %s requested\n",path);

    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_atime = time(NULL);
    st->st_mtime = time(NULL);

    if ( strcmp( path, "/"  ) == 0  )
    {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;

    }
    else
    {
        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
        st->st_size = 1024;
    }

    return 0;
}

#if EXAMPLE_FUSE

static struct fuse_operations test_oper = {
    .getattr	= test_getattr,
    .readdir    = test_readdir,
    .read       = test_read,
};

#endif

IOM *iom;

void *lab4_init(struct fuse_conn_info *conn)
{    
    lab4_fill_super(iom);
    // 
}

static int lab4_getattr(const char *path, struct stat *stbuf)
{	
    INODE *inode = NULL;
    int res;
    
    res = do_path_check(path);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;

    res = lab4_read_inode(iom, path, &inode);
    if(res == LAB4_ERROR)
        return LAB4_ERROR; 
    else if(res == -ENOENT)
        return -ENOENT;
    do_fill_inode(iom, inode, stbuf);

    return LAB4_SUCCESS;
}

static int lab4_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi)
{
    struct lab4_dir_entry *dir_entry;
    struct stat st;
    INODE *dir_inode = NULL;
	int res = -1;

    res = do_path_check(path);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;

    res = lab4_read_inode(iom, path, &dir_inode);
    if(res == LAB4_ERROR)
        return LAB4_ERROR; 
    else if(res == -ENOENT)
        return -ENOENT;

    res = do_readdir(iom,dir_inode, offset,buf,filler);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;

    return 0;
}


static int xmp_access(const char *path, int mask)
{
    return LAB4_SUCCESS;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
    return LAB4_SUCCESS;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
    return LAB4_SUCCESS;
}

static int lab4_mkdir(const char *path, mode_t mode)
{
    struct lab4_dir_entry *dir_entry;
    INODE *dir_inode = NULL;
    char f_name[FNAME_SIZE]={0.};
    char d_name[FNAME_SIZE]={0,};
	int res = -1;

    res = do_path_check(path);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;

    get_name_from_path(path, f_name, d_name);

    res = lab4_read_inode(iom, d_name, &dir_inode);
    if(res == LAB4_ERROR)
        return LAB4_ERROR; 
    else if(res == -ENOENT)
        return -ENOENT;

	if (strlen(f_name) < 1 || strlen(f_name) >= FNAME_SIZE) {
		return -ENAMETOOLONG;
	}

    res=do_mkdir(iom,dir_inode, f_name);
    if(res == LAB4_ERROR)
        return res;
    if(res == -EEXIST)
        return res;
    
    return LAB4_SUCCESS;
}

static int lab4_unlink(const char *path)
{
    struct lab4_dir_entry *dir_entry;
    INODE *dir_inode = NULL;
    char f_name[FNAME_SIZE]={0.};
    char d_name[FNAME_SIZE]={0,};
	int res = -1;

    res = do_path_check(path);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;

    res = get_name_from_path(path, f_name, d_name);

    res = lab4_read_inode(iom, d_name, &dir_inode);
    if(res == LAB4_ERROR)
        return res; 
    else if(res == -ENOENT)
        return res;

    res = do_unlink(iom,dir_inode, f_name);
    if(res == LAB4_ERROR)
        return res; 
    else if(res == -ENOENT)
        return -ENOENT;
    return res;
}

static int lab4_rmdir(const char *path)
{
    struct lab4_dir_entry *dir_entry;
    INODE *dir_inode = NULL;
    char f_name[FNAME_SIZE]={0.};
    char d_name[FNAME_SIZE]={0,};
	int res = -1;

    res = do_path_check(path);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;

    res = get_name_from_path(path, f_name, d_name);

    res = lab4_read_inode(iom, d_name, &dir_inode);
    if(res == LAB4_ERROR)
        return LAB4_ERROR; 
    else if(res == -ENOENT)
        return -ENOENT;

    res = do_rmdir(iom,dir_inode, f_name);
    if(res == LAB4_ERROR)
        return LAB4_ERROR;
    if(res == -ENOENT)
        return -ENOENT;
    if(res == -ENOTEMPTY)
        return -ENOTEMPTY;
    
    return LAB4_SUCCESS;
}

static int xmp_symlink(const char *from, const char *to)
{
    return LAB4_SUCCESS;
}

static int xmp_rename(const char *from, const char *to)
{

}

static int xmp_link(const char *from, const char *to)
{
    return LAB4_SUCCESS;
}

static int xmp_chmod(const char *path, mode_t mode)
{
    return LAB4_SUCCESS;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
    return LAB4_SUCCESS;
}

static int xmp_truncate(const char *path, off_t size)
{
    return LAB4_SUCCESS;
}

static int lab4_open(const char *path, struct fuse_file_info *fi)
{
  	int fd;

	fd = lab4_do_open(iom, path, fi->flags, 0);
	if (fd == LAB4_ERROR)
		return -errno;

	fi->fh = fd;
  
    return LAB4_SUCCESS;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi)
{

    return LAB4_SUCCESS;
}

static int xmp_write(const char *path, const char *buf, size_t size,
        off_t offset, struct fuse_file_info *fi)
{
    return LAB4_SUCCESS;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{

    return LAB4_SUCCESS;
}

static int lab4_release(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	lab4_do_close(iom, fi->fh);
    return LAB4_SUCCESS;
}

static int xmp_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
    return LAB4_SUCCESS;
}

static void xmp_destroy(void* tmp){
}

static int xmp_opendir(const char* path, struct fuse_file_info* fi){
    return LAB4_SUCCESS;
}

static int lab4_create (const char *path, mode_t mode, struct fuse_file_info *fi){
    
  	int fd;

	fd = lab4_do_open(iom, path, fi->flags, mode);
	if (fd == LAB4_ERROR)
		return -errno;

	fi->fh = fd;
  
    return LAB4_SUCCESS;

}

static int lab4_utimens(const char *path, const struct timespec ts[2])
{
	int res;

	res = do_utimens(iom, path, ts);
	if (res == LAB4_ERROR)
		return -errno;

	return LAB4_SUCCESS;
}

static int xmp_releasedir(const char *path, struct fuse_file_info *fi){
    return LAB4_SUCCESS;
}

static struct fuse_operations lab4_oper = {
    .init       = lab4_init,        // o  (optional)
    .getattr	= lab4_getattr,     // o  (optional)
    .access		= xmp_access,
    .readlink	= xmp_readlink,
    .readdir	= lab4_readdir,     // o
    .mknod		= xmp_mknod,
    .mkdir		= lab4_mkdir,       // o
    .symlink	= xmp_symlink,
    .unlink		= lab4_unlink,      // o
    .rmdir		= lab4_rmdir,       // o
    .rename		= xmp_rename,       // x
    .link		= xmp_link,
    .chmod		= xmp_chmod,
    .chown		= xmp_chown,
    .truncate	= xmp_truncate,     // x
    .open		= lab4_open,        // o
    .read		= xmp_read,         // x
    .write		= xmp_write,        // x
    .statfs		= xmp_statfs,
    .release	= lab4_release,     // o
	.releasedir	= xmp_releasedir,   // x  (optional)
    .fsync		= xmp_fsync,
    .destroy    = xmp_destroy,      // x  (optional)
    .opendir    = xmp_opendir,      // x  (optional)
    .create     = lab4_create,      // o ... same with open
    .utimens     = lab4_utimens     // o
};

#endif

int main(int argc, char *argv[])
{  
    int device_selected = 0; 
    int mountpoint_selected = 0;
    char op;
    char *dev_path = NULL, *mnt_path = NULL;
    int format_type = LAB4_FORMAT_MISSING;
    s32 ret;

    int argc_lab4;
    char **argv_lab4;
    char *dev = "/dev/sdc";
    
    if (argc < 3)
    {
        goto INVALID_ARGS;
    }

    if(argv[1] == NULL){
        printf(" input mount point error\n");
        goto INVALID_ARGS;
    }

    if(argv[2] == NULL){
        printf("input device name error\n");
        goto INVALID_ARGS;
    }

    printf("\n device %s is selected \n",dev);
    printf("\n check if it is formatted to lab or fat ...  \n");

    iom = check_formated(argv[argc-2], argv[argc-1]);
    if(iom != NULL){
        printf("\n---------------------    executin mount    --------------------\n");
        printf("  device name = %s \n", iom->dev_path);
        printf("  mount point = %s \n", iom->mnt_path);

        printf("  FUSE_USE_VERSION = %d \n\n", FUSE_USE_VERSION);
        umask(0);

        /*FIXME*/
#if DEBUG 
        int tmp_argc = argc-1;
        char **tmp_argv=(char**)malloc(sizeof(char*)*tmp_argc);
        int i;
        for(i=0 ; i< tmp_argc-1; i++){
            tmp_argv[i] = (char*)malloc(strlen(argv[i]));
            strcpy(tmp_argv[i],argv[i]);
        }
        tmp_argv[i] = (char*)malloc(strlen(argv[argc-1]));
        strcpy(tmp_argv[i], argv[argc-1]);
#endif

#if EXAMPLE_FUSE
        ret = fuse_main(tmp_argc, tmp_argv, &test_oper, NULL);
#else
        ret = fuse_main(tmp_argc, tmp_argv, &lab4_oper, NULL);
//        ret = fuse_main(argc, argv, &lab4_oper, NULL);
#endif
        if(ret < 0){
            printf("\n---------------------     mount failed     --------------------\n");
            goto ERROR;
        }   
//       /*FIXME*/
//        printf("\n---------------------    mount success     --------------------\n");

        return LAB4_SUCCESS;
    }

    if(iom)
        close_io_manager(iom);
    if(dev_path)
        free(dev_path);

    return LAB4_SUCCESS;

    if(iom != NULL)
        close_io_manager(iom);

ERROR:
    free_buf(dev_path);
    free_buf(mnt_path);
    free_buf((s8*)iom);
    
INVALID_ARGS:
    lab4_fuse_mount_usage(argv[0]);
    lab4_fuse_mount_example(argv[0]);

    return LAB4_INVALID_ARGS;
    
}


