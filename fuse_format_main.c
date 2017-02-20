/*
*	Operating System Lab
*	    Lab4 (File System in Userspace)
*	    Copyright (C) 2017 Juhyung Son <tooson9010@gmail.com>
*	    First Writing: 30/12/2016
*
*   fuse_format_main.c :
*       - main source to make lab4_format executable file.
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

int main(int argc, char *argv[])
{
    struct io_manager *iom = NULL;
    int need_format = 0; 
	char op;
    char *dev_path = NULL;
    int format_type = LAB4_FORMAT_MISSING;

	if (argc == 1)
	{
		goto INVALID_ARGS;
	}

	/* optind must be reset before using getopt() */
	optind = 0;
	while ((op = getopt(argc, argv, "d:f:")) != -1) {
		switch (op) {		
		case 'd':		
            need_format = 1;
            dev_path = (char*)malloc(sizeof(char)*strlen(optarg) + 1);
            memset(dev_path, 0, sizeof(char)*strlen(optarg) + 1);
            strcpy(dev_path, optarg);

			break;
		case 'f':
            if(!strcmp(optarg,"fat"))
                format_type = LAB4_FORMAT_FAT;
            else if(!strcmp(optarg,"lab"))
                format_type = LAB4_FORMAT_LAB;
			break;
		default:
			goto INVALID_ARGS;
		}
	}

    if(need_format){
        if(format_type != LAB4_FORMAT_MISSING){
            /* commit format */
            if(!(iom = set_io_manager(dev_path, O_RDWR))){
                printf("format error \n\n");
                goto INVALID_ARGS;
            } /* get device info (no_of_sector ...)  */
            
            fill_io_manager(iom,format_type);
            format_device(iom); // write 0 to device;
            goto SUCCESS;
        
        }else{    
            printf("INVALID FORMAT type : %s \n", optarg);
            printf("format type is missing (e.g. lab , fat)  \n");
            goto INVALID_ARGS;
        }
    }else{
        printf("device name is missing (e.g. /dev/sdc) \n");
        goto INVALID_ARGS;
    }
 
INVALID_ARGS:
       
    free_buf(dev_path);
	lab4_fuse_mkfs_usage(argv[0]);
    lab4_fuse_mkfs_example(argv[0]);

	return LAB4_INVALID_ARGS;
SUCCESS:
    if(iom)
        close_io_manager(iom);
    return LAB4_SUCCESS;

}


