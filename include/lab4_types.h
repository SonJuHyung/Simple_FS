#include<sys/stat.h>
#include "lab4_config.h"

#ifndef _LAB4_TYPES_H
#define _LAB4_TYPES_H


typedef signed char			s8;	
typedef signed short		s16;
typedef signed int			s32; 
typedef signed long         s64; 

typedef unsigned char		u8;	
typedef unsigned short		u16;
typedef unsigned int		u32; 
typedef unsigned long       u64; 

#define __O_SYNC        04000000


#ifndef O_DSYNC
#define O_DSYNC         00010000
#endif

#ifndef O_SYNC
#define __O_SYNC        04000000
#define O_SYNC			(__O_SYNC | O_DSYNC)
#endif

#ifndef O_DIRECT
#define O_DIRECT     0200000
#endif
		
typedef u32 inode_t;//inode number
typedef u32 pbno_t;//physical block number
typedef u32 lbno_t;//logical block num

#define LAB4_TYPE_DATA 0
#define LAB4_TYPE_META 1

#define CEIL(x, y)  ((x + y - 1) / y)
#define	MAX(x,y)	((x) > (y) ? (x) : (y))
#define	MIN(x,y)	((x) < (y) ? (x) : (y))

#define LAB4_ERROR -1
#define LAB4_SUCCESS 0
#define LAB4_OUT_OF_MEMORY -1
#define LAB4_DO_NOT_EXIST -2
#define LAB4_INVALID_ARGS -2

#define LAB4_FORMAT_MISSING 0
#define LAB4_FORMAT_LAB 1
#define LAB4_FORMAT_FAT 2


#define GET_SIZE(size) \
    sizeof(struct size)

#define READ 1
#define WRITE 0

#define DIRTY 1
#define CLEAN 0

#define FLUSH 1
#define UNFLUSH 0

#define LOCK 1
#define UNLOCK 0

#define READ_LOCK 1
#define WRITE_LOCK 0

#define SYNC 0
#define ASYNC 1

#define DATA 0
#define META 1

/* ERROR STATUS */
#define	LAB4_ERROR		-1
#define	LAB4_SUCCESS		0
#define	LAB4_FORMATERR	1

#define FALSE	0
#define TRUE	1

/* CAPACITY CALCULATION */
#define LAB4_TERA_BYTES ((u64)1024*1024*1024*1024)
#define LAB4_GIGA_BYTES (1024*1024*1024)
#define LAB4_MEGA_BYTES (1024*1024)
#define LAB4_KILO_BYTES (1024)

#endif 


