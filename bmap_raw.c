/*
*	Operating System Lab
*	    Lab4 (File System in Userspace)
*	    Copyright (C) 2017 Juhyung Son <tooson9010@gmail.com>
*	    First Writing: 30/12/2016
*
*   bmap_raw.c :
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

/* ibitmap set bit function  */
s32 lab4_set_bit(u32 nr,void * addr)
{
	s32		mask, retval;
	u8	*ADDR = (u8 *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	retval = mask & *ADDR;
	*ADDR |= mask;
	return retval;
}

s32 lab4_clear_bit(u32 nr, void * addr)
{
	s32		mask, retval;
	u8	*ADDR = (u8 *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	retval = mask & *ADDR;
	*ADDR &= ~mask;
	return retval;
}

s32 lab4_test_bit(u32 nr, const void * addr)
{
	s32			mask;
	const u8	*ADDR = (const u8 *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	return (mask & *ADDR);
}

