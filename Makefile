#
#
#
#
#

CC = gcc
INC = -l /home/son/dku/Lab/Lab4_fuse/Lab4/include
CFLAGS = -g -W $(INC)
CFLAGS += -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=30 -I/usr/include/fuse 

LDFLAGS = -lfuse 

OBJS_FUSE = fuse_main.o file.o dir.o inode.o disk_map.o fsformat.o mount.o bmap.o bmap_raw.o
OBJS_FORMAT = fuse_format_main.o fsformat.o bmap_raw.o

SRCS = $(OBJS_FUSE:.o=.c) $(OBJS_FORMAT:.o=.c)

TARGET_FUSE = lab4_mount
TARGET_FORMAT = lab4_mkfs

.SUFFIXES : .c .o

.c.o:
	@echo "Compiling $< ..."
	$(CC)  -g -c -D_GNU_SOURCE $(CFLAGS) -o $@ $<

$(TARGET_FUSE) : $(OBJS_FUSE)
	$(CC) -o $(TARGET_FUSE) $(OBJS_FUSE) $(LDFLAGS) 

$(TARGET_FORMAT) : $(OBJS_FORMAT)
	$(CC) -o $(TARGET_FORMAT) $(OBJS_FORMAT) 


all : $(TARGET_FUSE) $(TARGET_FORMAT)


dep : 
	gccmaedep $(INC) $(SRCS)

clean :
	@echo "Cleaning $< ..."
	rm -rf $(OBJS_FUSE) $(OBJS_FORMAT) $(TARGET_FUSE) $(TARGET_FORMAT)

new :
	$(MAKE) clean
	$(MAKE)
