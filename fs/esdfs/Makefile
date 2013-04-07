ESDFS_VERSION="0.1"

EXTRA_CFLAGS += -DESDFS_VERSION=\"$(ESDFS_VERSION)\"

obj-$(CONFIG_ESD_FS) += esdfs.o

esdfs-y := dentry.o file.o inode.o main.o super.o lookup.o mmap.o
