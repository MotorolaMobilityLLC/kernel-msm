/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copied from fs/f2fs/f2fs.h
 * This File includes necessary F2FS codes for Write Hint Detection.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 */

#include <linux/uio.h>
#include <linux/types.h>
#include <linux/page-flags.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/crc32.h>
#include <linux/magic.h>
#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/vmalloc.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/quotaops.h>
#include <linux/part_stat.h>
#include <crypto/hash.h>
#include <linux/fscrypt.h>
#include <linux/fsverity.h>

typedef u32 block_t;	/*
			 * should not change u32, since it is the on-disk block
			 * address format, __le32.
			 */
typedef u32 nid_t;
typedef __le32	f2fs_hash_t;
#define MAX_PATH_LEN            64
#define MAX_VOLUME_NAME         512
#define F2FS_MAX_EXTENSION              64      /* # of extension entries */
#define F2FS_EXTENSION_LEN              8       /* max size of extension */
#define VERSION_LEN     256
#define MAX_DEVICES             8
#define F2FS_MAX_QUOTAS         3
#define ENTRIES_IN_SUM          512
#define MAX_GC_POLICY		5
#define SUMMARY_SIZE            (7)     /* sizeof(struct summary) */
#define SUM_FOOTER_SIZE         (5)     /* sizeof(struct summary_footer) */
#define SUM_ENTRY_SIZE          (SUMMARY_SIZE * ENTRIES_IN_SUM)
#define F2FS_BLKSIZE                    4096    /* support only 4KB block */
#define MAX_NAT_STATE		3
#define MAX_NID_STATE		2
#define MAX_TIME		6
#define NR_INODE_TYPE		4
#define NR_COUNT_TYPE		14
#define COMPRESS_EXT_NUM                16
#define NR_CURSEG_DATA_TYPE     (3)
#define NR_CURSEG_NODE_TYPE     (3)
#define NR_CURSEG_INMEM_TYPE    (2)
#define NR_CURSEG_RO_TYPE       (2)
#define NR_CURSEG_PERSIST_TYPE  (NR_CURSEG_DATA_TYPE + NR_CURSEG_NODE_TYPE)
#define NR_CURSEG_TYPE          (NR_CURSEG_INMEM_TYPE + NR_CURSEG_PERSIST_TYPE)
#define MAX_INO_ENTRY		5
#define META_MAX		4
#define MAX_GC_MODE		6
#define SUM_JOURNAL_SIZE        (F2FS_BLKSIZE - SUM_FOOTER_SIZE -\
		SUM_ENTRY_SIZE)
#define NAT_JOURNAL_ENTRIES     ((SUM_JOURNAL_SIZE - 2) /\
		sizeof(struct nat_journal_entry))
#define NAT_JOURNAL_RESERVED    ((SUM_JOURNAL_SIZE - 2) %\
		sizeof(struct nat_journal_entry))
#define SIT_JOURNAL_ENTRIES     ((SUM_JOURNAL_SIZE - 2) /\
		sizeof(struct sit_journal_entry))
#define SIT_JOURNAL_RESERVED    ((SUM_JOURNAL_SIZE - 2) %\
		sizeof(struct sit_journal_entry))
#define EXTRA_INFO_RESERVED     (SUM_JOURNAL_SIZE - 2 - 8)
#define SIT_VBLOCK_MAP_SIZE 64
#define NIDS_PER_BLOCK		1018
#define DEF_ADDRS_PER_INODE	923
#define DEF_NIDS_PER_INODE	5
#define DEF_ADDRS_PER_BLOCK	1018
#define F2FS_NAME_LEN		255
#define is_cold_node(page)	is_node(page, COLD_BIT_SHIFT)
#define FADVISE_HOT_BIT		0x20
#define file_is_hot(inode)	is_file(inode, FADVISE_HOT_BIT)
#define FADVISE_COLD_BIT	0x01
#define file_is_cold(inode)	is_file(inode, FADVISE_COLD_BIT)
#define F2FS_OPTION(sbi)	((sbi)->mount_opt)

enum page_type {
	DATA,
	NODE,
	META,
	NR_PAGE_TYPE,
	META_FLUSH,
	INMEM,          /* the below types are used by tracepoints only. */
	INMEM_DROP,
	INMEM_INVALIDATE,
	INMEM_REVOKE,
	IPU,
	OPU,
};

enum temp_type {
	HOT = 0,        /* must be zero for meta bio */
	WARM,
	COLD,
	NR_TEMP_TYPE,
};

enum iostat_type {
	/* WRITE IO */
	APP_DIRECT_IO,                  /* app direct write IOs */
	APP_BUFFERED_IO,                /* app buffered write IOs */
	APP_WRITE_IO,                   /* app write IOs */
	APP_MAPPED_IO,                  /* app mapped IOs */
	FS_DATA_IO,                     /* data IOs from kworker/fsync/reclaimer */
	FS_NODE_IO,                     /* node IOs from kworker/fsync/reclaimer */
	FS_META_IO,                     /* meta IOs from kworker/reclaimer */
	FS_GC_DATA_IO,                  /* data IOs from forground gc */
	FS_GC_NODE_IO,                  /* node IOs from forground gc */
	FS_CP_DATA_IO,                  /* data IOs from checkpoint */
	FS_CP_NODE_IO,                  /* node IOs from checkpoint */
	FS_CP_META_IO,                  /* meta IOs from checkpoint */

	/* READ IO */
	APP_DIRECT_READ_IO,             /* app direct read IOs */
	APP_BUFFERED_READ_IO,           /* app buffered read IOs */
	APP_READ_IO,                    /* app read IOs */
	APP_MAPPED_READ_IO,             /* app mapped read IOs */
	FS_DATA_READ_IO,                /* data read IOs */
	FS_GDATA_READ_IO,               /* data read IOs from background gc */
	FS_CDATA_READ_IO,               /* compressed data read IOs */
	FS_NODE_READ_IO,                /* node read IOs */
	FS_META_READ_IO,                /* meta read IOs */

	/* other */
	FS_DISCARD,                     /* discard */
	NR_IO_TYPE,
};

enum {
	PAGE_PRIVATE_NOT_POINTER,		/* private contains non-pointer data */
	PAGE_PRIVATE_ATOMIC_WRITE,		/* data page from atomic write path */
	PAGE_PRIVATE_DUMMY_WRITE,		/* data page for padding aligned IO */
	PAGE_PRIVATE_ONGOING_MIGRATION,		/* data page which is on-going migrating */
	PAGE_PRIVATE_INLINE_INODE,		/* inode page contains inline data */
	PAGE_PRIVATE_REF_RESOURCE,		/* dirty page has referenced resources */
	PAGE_PRIVATE_MAX
};

/* Notice: The order of dirty type is same with CURSEG_XXX in f2fs.h */
enum dirty_type {
	DIRTY_HOT_DATA,		/* dirty segments assigned as hot data logs */
	DIRTY_WARM_DATA,	/* dirty segments assigned as warm data logs */
	DIRTY_COLD_DATA,	/* dirty segments assigned as cold data logs */
	DIRTY_HOT_NODE,		/* dirty segments assigned as hot node logs */
	DIRTY_WARM_NODE,	/* dirty segments assigned as warm node logs */
	DIRTY_COLD_NODE,	/* dirty segments assigned as cold node logs */
	DIRTY,			/* to count # of dirty segments */
	PRE,			/* to count # of entirely obsolete segments */
	NR_DIRTY_TYPE
};

/* used for f2fs_inode_info->flags */
enum {
	FI_NEW_INODE,		/* indicate newly allocated inode */
	FI_DIRTY_INODE,		/* indicate inode is dirty or not */
	FI_AUTO_RECOVER,	/* indicate inode is recoverable */
	FI_DIRTY_DIR,		/* indicate directory has dirty pages */
	FI_INC_LINK,		/* need to increment i_nlink */
	FI_ACL_MODE,		/* indicate acl mode */
	FI_NO_ALLOC,		/* should not allocate any blocks */
	FI_FREE_NID,		/* free allocated nide */
	FI_NO_EXTENT,		/* not to use the extent cache */
	FI_INLINE_XATTR,	/* used for inline xattr */
	FI_INLINE_DATA,		/* used for inline data*/
	FI_INLINE_DENTRY,	/* used for inline dentry */
	FI_APPEND_WRITE,	/* inode has appended data */
	FI_UPDATE_WRITE,	/* inode has in-place-update data */
	FI_NEED_IPU,		/* used for ipu per file */
	FI_ATOMIC_FILE,		/* indicate atomic file */
	FI_ATOMIC_COMMIT,	/* indicate the state of atomical committing */
	FI_VOLATILE_FILE,	/* indicate volatile file */
	FI_FIRST_BLOCK_WRITTEN,	/* indicate #0 data block was written */
	FI_DROP_CACHE,		/* drop dirty page cache */
	FI_DATA_EXIST,		/* indicate data exists */
	FI_INLINE_DOTS,		/* indicate inline dot dentries */
	FI_DO_DEFRAG,		/* indicate defragment is running */
	FI_DIRTY_FILE,		/* indicate regular/symlink has dirty pages */
	FI_NO_PREALLOC,		/* indicate skipped preallocated blocks */
	FI_HOT_DATA,		/* indicate file is hot */
	FI_EXTRA_ATTR,		/* indicate file has extra attribute */
	FI_PROJ_INHERIT,	/* indicate file inherits projectid */
	FI_PIN_FILE,		/* indicate file should not be gced */
	FI_ATOMIC_REVOKE_REQUEST, /* request to drop atomic data */
	FI_VERITY_IN_PROGRESS,	/* building fs-verity Merkle tree */
	FI_COMPRESSED_FILE,	/* indicate file's data can be compressed */
	FI_COMPRESS_CORRUPT,	/* indicate compressed cluster is corrupted */
	FI_MMAP_FILE,		/* indicate file was mmapped */
	FI_ENABLE_COMPRESS,	/* enable compression in "user" compression mode */
	FI_COMPRESS_RELEASED,	/* compressed blocks were released */
	FI_ALIGNED_WRITE,	/* enable aligned write */
	FI_MAX,			/* max flag, never be used */
};

enum {
	GC_FAILURE_PIN,
	GC_FAILURE_ATOMIC,
	MAX_GC_FAILURE
};

enum {
	COMPR_MODE_FS,		/*
						 * automatically compress compression
						 * enabled files
						 */
	COMPR_MODE_USER,	/*
						 * automatical compression is disabled.
						 * user can control the file compression
						 * using ioctls
						 */
};

/*
 * For superblock
 */
struct f2fs_device {
	__u8 path[MAX_PATH_LEN];
	__le32 total_segments;
} __packed;

struct f2fs_super_block {
	__le32 magic;			/* Magic Number */
	__le16 major_ver;		/* Major Version */
	__le16 minor_ver;		/* Minor Version */
	__le32 log_sectorsize;		/* log2 sector size in bytes */
	__le32 log_sectors_per_block;	/* log2 # of sectors per block */
	__le32 log_blocksize;		/* log2 block size in bytes */
	__le32 log_blocks_per_seg;	/* log2 # of blocks per segment */
	__le32 segs_per_sec;		/* # of segments per section */
	__le32 secs_per_zone;		/* # of sections per zone */
	__le32 checksum_offset;		/* checksum offset inside super block */
	__le64 block_count;		/* total # of user blocks */
	__le32 section_count;		/* total # of sections */
	__le32 segment_count;		/* total # of segments */
	__le32 segment_count_ckpt;	/* # of segments for checkpoint */
	__le32 segment_count_sit;	/* # of segments for SIT */
	__le32 segment_count_nat;	/* # of segments for NAT */
	__le32 segment_count_ssa;	/* # of segments for SSA */
	__le32 segment_count_main;	/* # of segments for main area */
	__le32 segment0_blkaddr;	/* start block address of segment 0 */
	__le32 cp_blkaddr;		/* start block address of checkpoint */
	__le32 sit_blkaddr;		/* start block address of SIT */
	__le32 nat_blkaddr;		/* start block address of NAT */
	__le32 ssa_blkaddr;		/* start block address of SSA */
	__le32 main_blkaddr;		/* start block address of main area */
	__le32 root_ino;		/* root inode number */
	__le32 node_ino;		/* node inode number */
	__le32 meta_ino;		/* meta inode number */
	__u8 uuid[16];			/* 128-bit uuid for volume */
	__le16 volume_name[MAX_VOLUME_NAME];	/* volume name */
	__le32 extension_count;		/* # of extensions below */
	__u8 extension_list[F2FS_MAX_EXTENSION][F2FS_EXTENSION_LEN];/* extension array */
	__le32 cp_payload;
	__u8 version[VERSION_LEN];	/* the kernel version */
	__u8 init_version[VERSION_LEN];	/* the initial kernel version */
	__le32 feature;			/* defined features */
	__u8 encryption_level;		/* versioning level for encryption */
	__u8 encrypt_pw_salt[16];	/* Salt used for string2key algorithm */
	struct f2fs_device devs[MAX_DEVICES];	/* device list */
	__le32 qf_ino[F2FS_MAX_QUOTAS];	/* quota inode numbers */
	__u8 hot_ext_count;		/* # of hot file extension */
	__le16  s_encoding;		/* Filename charset encoding */
	__le16  s_encoding_flags;	/* Filename charset encoding flags */
	__u8 reserved[306];		/* valid reserved region */
	__le32 crc;			/* checksum of superblock */
} __packed;

struct f2fs_mount_info {
	unsigned int opt;
	int write_io_size_bits;         /* Write IO size bits */
	block_t root_reserved_blocks;   /* root reserved blocks */
	kuid_t s_resuid;                /* reserved blocks for uid */
	kgid_t s_resgid;                /* reserved blocks for gid */
	int active_logs;                /* # of active logs */
	int inline_xattr_size;          /* inline xattr size */
#ifdef CONFIG_F2FS_FAULT_INJECTION
	struct f2fs_fault_info fault_info;      /* For fault injection */
#endif
#ifdef CONFIG_QUOTA
	/* Names of quota files with journalled quota */
	char *s_qf_names[MAXQUOTAS];
	int s_jquota_fmt;                       /* Format of quota to use */
#endif
	/* For which write hints are passed down to block layer */
	int whint_mode;
	int alloc_mode;                 /* segment allocation policy */
	int fsync_mode;                 /* fsync policy */
	int fs_mode;                    /* fs mode: LFS or ADAPTIVE */
	int bggc_mode;                  /* bggc mode: off, on or sync */
	struct fscrypt_dummy_policy dummy_enc_policy; /* test dummy encryption */
	block_t unusable_cap_perc;      /* percentage for cap */
	block_t unusable_cap;           /* Amount of space allowed to be
									 * unusable when disabling checkpoint
									 */

	/* For compression */
	unsigned char compress_algorithm;       /* algorithm type */
	unsigned char compress_log_size;        /* cluster log size */
	unsigned char compress_level;           /* compress level */
	bool compress_chksum;                   /* compressed data chksum */
	unsigned char compress_ext_cnt;         /* extension count */
	int compress_mode;                      /* compression mode */
	unsigned char extensions[COMPRESS_EXT_NUM][F2FS_EXTENSION_LEN]; /* extensions */
};

struct f2fs_nat_entry {
	__u8 version;           /* latest version of cached nat entry */
	__le32 ino;             /* inode number */
	__le32 block_addr;      /* block address */
} __packed;

struct f2fs_sit_entry {
	__le16 vblocks;                         /* reference above */
	__u8 valid_map[SIT_VBLOCK_MAP_SIZE];    /* bitmap for valid blocks */
	__le64 mtime;                           /* segment age for cleaning */
} __packed;

struct nat_journal_entry {
	__le32 nid;
	struct f2fs_nat_entry ne;
} __packed;

struct nat_journal {
	struct nat_journal_entry entries[NAT_JOURNAL_ENTRIES];
	__u8 reserved[NAT_JOURNAL_RESERVED];
} __packed;

struct sit_journal_entry {
	__le32 segno;
	struct f2fs_sit_entry se;
} __packed;

struct sit_journal {
	struct sit_journal_entry entries[SIT_JOURNAL_ENTRIES];
	__u8 reserved[SIT_JOURNAL_RESERVED];
} __packed;

struct f2fs_extra_info {
	__le64 kbytes_written;
	__u8 reserved[EXTRA_INFO_RESERVED];
} __packed;


struct f2fs_journal {
	union {
		__le16 n_nats;
		__le16 n_sits;
	};
	/* spare area is used by NAT or SIT journals or extra info */
	union {
		struct nat_journal nat_j;
		struct sit_journal sit_j;
		struct f2fs_extra_info info;
	};
} __packed;

/* a summary entry for a 4KB-sized block in a segment */
struct f2fs_summary {
	__le32 nid;             /* parent node id */
	union {
		__u8 reserved[3];
		struct {
			__u8 version;           /* node version number */
			__le16 ofs_in_node;     /* block index in parent node */
		} __packed;
	};
} __packed;

struct summary_footer {
	unsigned char entry_type;       /* SUM_TYPE_XXX */
	__le32 check_sum;               /* summary checksum */
} __packed;

/* 4KB-sized summary block structure */
struct f2fs_summary_block {
	struct f2fs_summary entries[ENTRIES_IN_SUM];
	struct f2fs_journal journal;
	struct summary_footer footer;
} __packed;

struct seg_entry {
	unsigned int type : 6;		/* segment type like CURSEG_XXX_TYPE */
	unsigned int valid_blocks : 10;	/* # of valid blocks */
	unsigned int ckpt_valid_blocks : 10;	/* # of valid blocks last cp */
	unsigned int padding : 6;		/* padding */
	unsigned char *cur_valid_map;	/* validity bitmap of blocks */
#ifdef CONFIG_F2FS_CHECK_FS
	unsigned char *cur_valid_map_mir;	/* mirror of current valid bitmap */
#endif
	/*
	 * # of valid blocks and the validity bitmap stored in the last
	 * checkpoint pack. This information is used by the SSR mode.
	 */
	unsigned char *ckpt_valid_map;	/* validity bitmap of blocks last cp */
	unsigned char *discard_map;
	unsigned long long mtime;	/* modification time of the segment */
};

struct sec_entry {
	unsigned int valid_blocks;	/* # of valid blocks in a section */
};

struct segment_allocation {
	void (*allocate_segment)(int, bool);
};

struct inmem_pages {
	struct list_head list;
	struct page *page;
	block_t old_addr;		/* for revoking when fail to commit */
};

struct sit_info {
	const struct segment_allocation *s_ops;

	block_t sit_base_addr;		/* start block address of SIT area */
	block_t sit_blocks;		/* # of blocks used by SIT area */
	block_t written_valid_blocks;	/* # of valid blocks in main area */
	char *bitmap;			/* all bitmaps pointer */
	char *sit_bitmap;		/* SIT bitmap pointer */
#ifdef CONFIG_F2FS_CHECK_FS
	char *sit_bitmap_mir;		/* SIT bitmap mirror */

	/* bitmap of segments to be ignored by GC in case of errors */
	unsigned long *invalid_segmap;
#endif
	unsigned int bitmap_size;	/* SIT bitmap size */

	unsigned long *tmp_map;			/* bitmap for temporal use */
	unsigned long *dirty_sentries_bitmap;	/* bitmap for dirty sentries */
	unsigned int dirty_sentries;		/* # of dirty sentries */
	unsigned int sents_per_block;		/* # of SIT entries per block */
	struct rw_semaphore sentry_lock;	/* to protect SIT cache */
	struct seg_entry *sentries;		/* SIT segment-level cache */
	struct sec_entry *sec_entries;		/* SIT section-level cache */

	/* for cost-benefit algorithm in cleaning procedure */
	unsigned long long elapsed_time;	/* elapsed time after mount */
	unsigned long long mounted_time;	/* mount time */
	unsigned long long min_mtime;		/* min. modification time */
	unsigned long long max_mtime;		/* max. modification time */
	unsigned long long dirty_min_mtime;	/* rerange candidates in GC_AT */
	unsigned long long dirty_max_mtime;	/* rerange candidates in GC_AT */

	unsigned int last_victim[MAX_GC_POLICY]; /* last victim segment # */
};

struct free_segmap_info {
	unsigned int start_segno;	/* start segment number logically */
	unsigned int free_segments;	/* # of free segments */
	unsigned int free_sections;	/* # of free sections */
	spinlock_t segmap_lock;		/* free segmap lock */
	unsigned long *free_segmap;	/* free segment bitmap */
	unsigned long *free_secmap;	/* free section bitmap */
};

struct dirty_seglist_info {
	const struct victim_selection *v_ops;	/* victim selction operation */
	unsigned long *dirty_segmap[NR_DIRTY_TYPE];
	unsigned long *dirty_secmap;
	struct mutex seglist_lock;		/* lock for segment bitmaps */
	int nr_dirty[NR_DIRTY_TYPE];		/* # of dirty segments */
	unsigned long *victim_secmap;		/* background GC victims */
};

/* victim selection function for cleaning and SSR */
struct victim_selection {
	int (*get_victim)(unsigned int*,
			int, int, char, unsigned long long);
};

/* for active log information */
struct curseg_info {
	struct mutex curseg_mutex;		/* lock for consistency */
	struct f2fs_summary_block *sum_blk;	/* cached summary block */
	struct rw_semaphore journal_rwsem;	/* protect journal area */
	struct f2fs_journal *journal;		/* cached journal info */
	unsigned char alloc_type;		/* current allocation type */
	unsigned short seg_type;		/* segment type like CURSEG_XXX_TYPE */
	unsigned int segno;			/* current segment number */
	unsigned short next_blkoff;		/* next block offset to write */
	unsigned int zone;			/* current zone number */
	unsigned int next_segno;		/* preallocated segment */
	bool inited;				/* indicate inmem log is inited */
};

struct sit_entry_set {
	struct list_head set_list;	/* link with all sit sets */
	unsigned int start_segno;	/* start segno of sits in set */
	unsigned int entry_cnt;		/* the # of sit entries in set */
};

struct f2fs_dev_info {
	struct block_device *bdev;
	char path[MAX_PATH_LEN];
	unsigned int total_segments;
	block_t start_blk;
	block_t end_blk;
#ifdef CONFIG_BLK_DEV_ZONED
	unsigned int nr_blkz;		/* Total number of zones */
	unsigned long *blkz_seq;	/* Bitmap indicating sequential zones */
	block_t *zone_capacity_blocks;  /* Array of zone capacity in blks */
#endif
};

struct f2fs_io_info {
	struct f2fs_sb_info *sbi;	/* f2fs_sb_info pointer */
	nid_t ino;		/* inode number */
	enum page_type type;	/* contains DATA/NODE/META/META_FLUSH */
	enum temp_type temp;	/* contains HOT/WARM/COLD */
	int op;			/* contains REQ_OP_ */
	int op_flags;		/* req_flag_bits */
	block_t new_blkaddr;	/* new block address to be written */
	block_t old_blkaddr;	/* old block address before Cow */
	struct page *page;	/* page to be written */
	struct page *encrypted_page;	/* encrypted page */
	struct page *compressed_page;	/* compressed page */
	struct list_head list;		/* serialize IOs */
	bool submitted;		/* indicate IO submission */
	int need_lock;		/* indicate we need to lock cp_rwsem */
	bool in_list;		/* indicate fio is in io_list */
	bool is_por;		/* indicate IO is from recovery or not */
	bool retry;		/* need to reallocate block address */
	int compr_blocks;	/* # of compressed block addresses */
	bool encrypted;		/* indicate file is encrypted */
	enum iostat_type io_type;	/* io type */
	struct writeback_control *io_wbc; /* writeback control */
	struct bio **bio;		/* bio for ipu */
	sector_t *last_block;		/* last block number in bio */
	unsigned char version;		/* version of the node */
};

struct f2fs_bio_info {
	struct f2fs_sb_info *sbi;	/* f2fs superblock */
	struct bio *bio;		/* bios to merge */
	sector_t last_block_in_bio;	/* last block number */
	struct f2fs_io_info fio;	/* store buffered io info. */
	struct rw_semaphore io_rwsem;	/* blocking op for bio */
	spinlock_t io_lock;		/* serialize DATA/NODE IOs */
	struct list_head io_list;	/* track fios */
	struct list_head bio_list;	/* bio entry list head */
	struct rw_semaphore bio_list_lock;	/* lock to protect bio entry list */
};

struct f2fs_sm_info {
	struct sit_info *sit_info;		/* whole segment information */
	struct free_segmap_info *free_info;	/* free segment information */
	struct dirty_seglist_info *dirty_info;	/* dirty segment information */
	struct curseg_info *curseg_array;	/* active segment information */

	struct rw_semaphore curseg_lock;	/* for preventing curseg change */

	block_t seg0_blkaddr;		/* block address of 0'th segment */
	block_t main_blkaddr;		/* start block address of main area */
	block_t ssa_blkaddr;		/* start block address of SSA area */

	unsigned int segment_count;	/* total # of segments */
	unsigned int main_segments;	/* # of segments in main area */
	unsigned int reserved_segments;	/* # of reserved segments */
	unsigned int ovp_segments;	/* # of overprovision segments */

	/* a threshold to reclaim prefree segments */
	unsigned int rec_prefree_segments;

	/* for batched trimming */
	unsigned int trim_sections;		/* # of sections to trim */

	struct list_head sit_entry_set;	/* sit entry set list */

	unsigned int ipu_policy;	/* in-place-update policy */
	unsigned int min_ipu_util;	/* in-place-update threshold */
	unsigned int min_fsync_blocks;	/* threshold for fsync */
	unsigned int min_seq_blocks;	/* threshold for sequential blocks */
	unsigned int min_hot_blocks;	/* threshold for hot block allocation */
	unsigned int min_ssr_sections;	/* threshold to trigger SSR allocation */

	/* for flush command control */
	struct flush_cmd_control *fcc_info;

	/* for discard command control */
	struct discard_cmd_control *dcc_info;
};

/* for inner inode cache management */
struct inode_management {
	struct radix_tree_root ino_root;        /* ino entry array */
	spinlock_t ino_lock;                    /* for ino entry lock */
	struct list_head ino_list;              /* inode list head */
	unsigned long ino_num;                  /* number of entries */
};

/* for GC_AT */
struct atgc_management {
	bool atgc_enabled;                      /* ATGC is enabled or not */
	struct rb_root_cached root;             /* root of victim rb-tree */
	struct list_head victim_list;           /* linked with all victim entries */
	unsigned int victim_count;              /* victim count in rb-tree */
	unsigned int candidate_ratio;           /* candidate ratio */
	unsigned int max_candidate_count;       /* max candidate count */
	unsigned int age_weight;                /* age weight, vblock_weight = 100 - age_weight */
	unsigned long long age_threshold;       /* age threshold */
};

struct ckpt_req_control {
	struct task_struct *f2fs_issue_ckpt;    /* checkpoint task */
	int ckpt_thread_ioprio;                 /* checkpoint merge thread ioprio */
	wait_queue_head_t ckpt_wait_queue;      /* waiting queue for wake-up */
	atomic_t issued_ckpt;           /* # of actually issued ckpts */
	atomic_t total_ckpt;            /* # of total ckpts */
	atomic_t queued_ckpt;           /* # of queued ckpts */
	struct llist_head issue_list;   /* list for command issue */
	spinlock_t stat_lock;           /* lock for below checkpoint time stats */
	unsigned int cur_time;          /* cur wait time in msec for currently issued checkpoint */
	unsigned int peak_time;         /* peak wait time in msec until now */
};

struct f2fs_nm_info {
	block_t nat_blkaddr;		/* base disk address of NAT */
	nid_t max_nid;			/* maximum possible node ids */
	nid_t available_nids;		/* # of available node ids */
	nid_t next_scan_nid;		/* the next nid to be scanned */
	unsigned int ram_thresh;	/* control the memory footprint */
	unsigned int ra_nid_pages;	/* # of nid pages to be readaheaded */
	unsigned int dirty_nats_ratio;	/* control dirty nats ratio threshold */

	/* NAT cache management */
	struct radix_tree_root nat_root;/* root of the nat entry cache */
	struct radix_tree_root nat_set_root;/* root of the nat set cache */
	struct rw_semaphore nat_tree_lock;	/* protect nat entry tree */
	struct list_head nat_entries;	/* cached nat entry list (clean) */
	spinlock_t nat_list_lock;	/* protect clean nat entry list */
	unsigned int nat_cnt[MAX_NAT_STATE]; /* the # of cached nat entries */
	unsigned int nat_blocks;	/* # of nat blocks */

	/* free node ids management */
	struct radix_tree_root free_nid_root;/* root of the free_nid cache */
	struct list_head free_nid_list;		/* list for free nids excluding preallocated nids */
	unsigned int nid_cnt[MAX_NID_STATE];	/* the number of free node id */
	spinlock_t nid_list_lock;	/* protect nid lists ops */
	struct mutex build_lock;	/* lock for build free nids */
	unsigned char **free_nid_bitmap;
	unsigned char *nat_block_bitmap;
	unsigned short *free_nid_count;	/* free nid count of NAT block */

	/* for checkpoint */
	char *nat_bitmap;		/* NAT bitmap pointer */

	unsigned int nat_bits_blocks;	/* # of nat bits blocks */
	unsigned char *nat_bits;	/* NAT bits blocks */
	unsigned char *full_nat_bits;	/* full NAT pages */
	unsigned char *empty_nat_bits;	/* empty NAT pages */
#ifdef CONFIG_F2FS_CHECK_FS
	char *nat_bitmap_mir;		/* NAT bitmap mirror */
#endif
	int bitmap_size;		/* bitmap size */
};

struct f2fs_sb_info {
	struct super_block *sb;			/* pointer to VFS super block */
	struct proc_dir_entry *s_proc;		/* proc entry */
	struct f2fs_super_block *raw_super;	/* raw super block pointer */
	struct rw_semaphore sb_lock;		/* lock for raw super block */
	int valid_super_block;			/* valid super block no */
	unsigned long s_flag;				/* flags for sbi */
	struct mutex writepages;		/* mutex for writepages() */

#ifdef CONFIG_BLK_DEV_ZONED
	unsigned int blocks_per_blkz;		/* F2FS blocks per zone */
	unsigned int log_blocks_per_blkz;	/* log2 F2FS blocks per zone */
#endif

	/* for node-related operations */
	struct f2fs_nm_info *nm_info;		/* node manager */
	struct inode *node_inode;		/* cache node blocks */

	/* for segment-related operations */
	struct f2fs_sm_info *sm_info;		/* segment manager */

	/* for bio operations */
	struct f2fs_bio_info *write_io[NR_PAGE_TYPE];	/* for write bios */
	/* keep migration IO order for LFS mode */
	struct rw_semaphore io_order_lock;
	mempool_t *write_io_dummy;		/* Dummy pages */

	/* for checkpoint */
	struct f2fs_checkpoint *ckpt;		/* raw checkpoint pointer */
	int cur_cp_pack;			/* remain current cp pack */
	spinlock_t cp_lock;			/* for flag in ckpt */
	struct inode *meta_inode;		/* cache meta blocks */
	struct rw_semaphore cp_global_sem;	/* checkpoint procedure lock */
	struct rw_semaphore cp_rwsem;		/* blocking FS operations */
	struct rw_semaphore node_write;		/* locking node writes */
	struct rw_semaphore node_change;	/* locking node change */
	wait_queue_head_t cp_wait;
	unsigned long last_time[MAX_TIME];	/* to store time in jiffies */
	long interval_time[MAX_TIME];		/* to store thresholds */
	struct ckpt_req_control cprc_info;	/* for checkpoint request control */

	struct inode_management im[MAX_INO_ENTRY];	/* manage inode cache */

	spinlock_t fsync_node_lock;		/* for node entry lock */
	struct list_head fsync_node_list;	/* node list head */
	unsigned int fsync_seg_id;		/* sequence id */
	unsigned int fsync_node_num;		/* number of node entries */

	/* for orphan inode, use 0'th array */
	unsigned int max_orphans;		/* max orphan inodes */

	/* for inode management */
	struct list_head inode_list[NR_INODE_TYPE];	/* dirty inode list */
	spinlock_t inode_lock[NR_INODE_TYPE];	/* for dirty inode list lock */
	struct mutex flush_lock;		/* for flush exclusion */

	/* for extent tree cache */
	struct radix_tree_root extent_tree_root;/* cache extent cache entries */
	struct mutex extent_tree_lock;	/* locking extent radix tree */
	struct list_head extent_list;		/* lru list for shrinker */
	spinlock_t extent_lock;			/* locking extent lru list */
	atomic_t total_ext_tree;		/* extent tree count */
	struct list_head zombie_list;		/* extent zombie tree list */
	atomic_t total_zombie_tree;		/* extent zombie tree count */
	atomic_t total_ext_node;		/* extent info count */

	/* basic filesystem units */
	unsigned int log_sectors_per_block;	/* log2 sectors per block */
	unsigned int log_blocksize;		/* log2 block size */
	unsigned int blocksize;			/* block size */
	unsigned int root_ino_num;		/* root inode number*/
	unsigned int node_ino_num;		/* node inode number*/
	unsigned int meta_ino_num;		/* meta inode number*/
	unsigned int log_blocks_per_seg;	/* log2 blocks per segment */
	unsigned int blocks_per_seg;		/* blocks per segment */
	unsigned int segs_per_sec;		/* segments per section */
	unsigned int secs_per_zone;		/* sections per zone */
	unsigned int total_sections;		/* total section count */
	unsigned int total_node_count;		/* total node block count */
	unsigned int total_valid_node_count;	/* valid node block count */
	int dir_level;				/* directory level */
	int readdir_ra;				/* readahead inode in readdir */
	u64 max_io_bytes;			/* max io bytes to merge IOs */

	block_t user_block_count;		/* # of user blocks */
	block_t total_valid_block_count;	/* # of valid blocks */
	block_t discard_blks;			/* discard command candidats */
	block_t last_valid_block_count;		/* for recovery */
	block_t reserved_blocks;		/* configurable reserved blocks */
	block_t current_reserved_blocks;	/* current reserved blocks */

	/* Additional tracking for no checkpoint mode */
	block_t unusable_block_count;		/* # of blocks saved by last cp */

	unsigned int nquota_files;		/* # of quota sysfile */
	struct rw_semaphore quota_sem;		/* blocking cp for flags */

	/* # of pages, see count_type */
	atomic_t nr_pages[NR_COUNT_TYPE];
	/* # of allocated blocks */
	struct percpu_counter alloc_valid_block_count;

	/* writeback control */
	atomic_t wb_sync_req[META];	/* count # of WB_SYNC threads */

	/* valid inode count */
	struct percpu_counter total_valid_inode_count;

	struct f2fs_mount_info mount_opt;	/* mount options */

	/* for cleaning operations */
	struct rw_semaphore gc_lock;		/*
										 * semaphore for GC, avoid
										 * race between GC and GC or CP
										 */
	struct f2fs_gc_kthread	*gc_thread;	/* GC thread */
	struct atgc_management am;		/* atgc management */
	unsigned int cur_victim_sec;		/* current victim section num */
	unsigned int gc_mode;			/* current GC state */
	unsigned int next_victim_seg[2];	/* next segment in victim section */

	/* for skip statistic */
	unsigned int atomic_files;		/* # of opened atomic file */
	unsigned long long skipped_atomic_files[2];	/* FG_GC and BG_GC */
	unsigned long long skipped_gc_rwsem;		/* FG_GC only */

	/* threshold for gc trials on pinned files */
	u64 gc_pin_file_threshold;
	struct rw_semaphore pin_sem;

	/* maximum # of trials to find a victim segment for SSR and GC */
	unsigned int max_victim_search;
	/* migration granularity of garbage collection, unit: segment */
	unsigned int migration_granularity;

	/*
	 * for stat information.
	 * one is for the LFS mode, and the other is for the SSR mode.
	 */
#ifdef CONFIG_F2FS_STAT_FS
	struct f2fs_stat_info *stat_info;	/* FS status information */
	atomic_t meta_count[META_MAX];		/* # of meta blocks */
	unsigned int segment_count[2];		/* # of allocated segments */
	unsigned int block_count[2];		/* # of allocated blocks */
	atomic_t inplace_count;		/* # of inplace update */
	atomic64_t total_hit_ext;		/* # of lookup extent cache */
	atomic64_t read_hit_rbtree;		/* # of hit rbtree extent node */
	atomic64_t read_hit_largest;		/* # of hit largest extent node */
	atomic64_t read_hit_cached;		/* # of hit cached extent node */
	atomic_t inline_xattr;			/* # of inline_xattr inodes */
	atomic_t inline_inode;			/* # of inline_data inodes */
	atomic_t inline_dir;			/* # of inline_dentry inodes */
	atomic_t compr_inode;			/* # of compressed inodes */
	atomic64_t compr_blocks;		/* # of compressed blocks */
	atomic_t vw_cnt;			/* # of volatile writes */
	atomic_t max_aw_cnt;			/* max # of atomic writes */
	atomic_t max_vw_cnt;			/* max # of volatile writes */
	unsigned int io_skip_bggc;		/* skip background gc for in-flight IO */
	unsigned int other_skip_bggc;		/* skip background gc for other reasons */
	unsigned int ndirty_inode[NR_INODE_TYPE];	/* # of dirty inodes */
#endif
	spinlock_t stat_lock;			/* lock for stat operations */

	/* to attach REQ_META|REQ_FUA flags */
	unsigned int data_io_flag;
	unsigned int node_io_flag;

	/* For sysfs suppport */
	struct kobject s_kobj;			/* /sys/fs/f2fs/<devname> */
	struct completion s_kobj_unregister;

	struct kobject s_stat_kobj;		/* /sys/fs/f2fs/<devname>/stat */
	struct completion s_stat_kobj_unregister;

	struct kobject s_feature_list_kobj;		/* /sys/fs/f2fs/<devname>/feature_list */
	struct completion s_feature_list_kobj_unregister;

	/* For shrinker support */
	struct list_head s_list;
	int s_ndevs;				/* number of devices */
	struct f2fs_dev_info *devs;		/* for device list */
	unsigned int dirty_device;		/* for checkpoint data flush */
	spinlock_t dev_lock;			/* protect dirty_device */
	struct mutex umount_mutex;
	unsigned int shrinker_run_no;

	/* For write statistics */
	u64 sectors_written_start;
	u64 kbytes_written;

	/* Reference to checksum algorithm driver via cryptoapi */
	struct crypto_shash *s_chksum_driver;

	/* Precomputed FS UUID checksum for seeding other checksums :*/
	__u32 s_chksum_seed;

	struct workqueue_struct *post_read_wq;	/* post read workqueue */

	struct kmem_cache *inline_xattr_slab;	/* inline xattr entry */
	unsigned int inline_xattr_slab_size;	/* default inline xattr slab size */

	/* For reclaimed segs statistics per each GC mode */
	unsigned int gc_segment_mode;		/* GC state for reclaimed segments */
	unsigned int gc_reclaimed_segs[MAX_GC_MODE];	/* Reclaimed segs for each mode */

	unsigned long seq_file_ra_mul;		/* multiplier for ra_pages of seq. files in fadvise */

#ifdef CONFIG_F2FS_FS_COMPRESSION
	struct kmem_cache *page_array_slab;	/* page array entry */
	unsigned int page_array_slab_size;	/* default page array slab size */

	/* For runtime compression statistics */
	u64 compr_written_block;
	u64 compr_saved_block;
	u32 compr_new_inode;

	/* For compressed block cache */
	struct inode *compress_inode;		/* cache compressed blocks */
	unsigned int compress_percent;		/* cache page percentage */
	unsigned int compress_watermark;	/* cache page watermark */
	atomic_t compress_page_hit;		/* cache hit count */
#endif

#ifdef CONFIG_F2FS_IOSTAT
	/* For app/fs IO statistics */
	spinlock_t iostat_lock;
	unsigned long long rw_iostat[NR_IO_TYPE];
	unsigned long long prev_rw_iostat[NR_IO_TYPE];
	bool iostat_enable;
	unsigned long iostat_next_period;
	unsigned int iostat_period_ms;

	/* For io latency related statistics info in one iostat period */
	spinlock_t iostat_lat_lock;
	struct iostat_lat_info *iostat_io_lat;
#endif
};

struct f2fs_stat_info {
	struct list_head stat_list;
	struct f2fs_sb_info *sbi;
	int all_area_segs, sit_area_segs, nat_area_segs, ssa_area_segs;
	int main_area_segs, main_area_sections, main_area_zones;
	unsigned long long hit_largest, hit_cached, hit_rbtree;
	unsigned long long hit_total, total_ext;
	int ext_tree, zombie_tree, ext_node;
	int ndirty_node, ndirty_dent, ndirty_meta, ndirty_imeta;
	int ndirty_data, ndirty_qdata;
	int inmem_pages;
	unsigned int ndirty_dirs, ndirty_files, nquota_files, ndirty_all;
	int nats, dirty_nats, sits, dirty_sits;
	int free_nids, avail_nids, alloc_nids;
	int total_count, utilization;
	int bg_gc, nr_wb_cp_data, nr_wb_data;
	int nr_rd_data, nr_rd_node, nr_rd_meta;
	int nr_dio_read, nr_dio_write;
	unsigned int io_skip_bggc, other_skip_bggc;
	int nr_flushing, nr_flushed, flush_list_empty;
	int nr_discarding, nr_discarded;
	int nr_discard_cmd;
	unsigned int undiscard_blks;
	int nr_issued_ckpt, nr_total_ckpt, nr_queued_ckpt;
	unsigned int cur_ckpt_time, peak_ckpt_time;
	int inline_xattr, inline_inode, inline_dir, append, update, orphans;
	int compr_inode;
	unsigned long long compr_blocks;
	int aw_cnt, max_aw_cnt, vw_cnt, max_vw_cnt;
	unsigned int valid_count, valid_node_count, valid_inode_count, discard_blks;
	unsigned int bimodal, avg_vblocks;
	int util_free, util_valid, util_invalid;
	int rsvd_segs, overp_segs;
	int dirty_count, node_pages, meta_pages, compress_pages;
	int compress_page_hit;
	int prefree_count, call_count, cp_count, bg_cp_count;
	int tot_segs, node_segs, data_segs, free_segs, free_secs;
	int bg_node_segs, bg_data_segs;
	int tot_blks, data_blks, node_blks;
	int bg_data_blks, bg_node_blks;
	unsigned long long skipped_atomic_files[2];
	int curseg[NR_CURSEG_TYPE];
	int cursec[NR_CURSEG_TYPE];
	int curzone[NR_CURSEG_TYPE];
	unsigned int dirty_seg[NR_CURSEG_TYPE];
	unsigned int full_seg[NR_CURSEG_TYPE];
	unsigned int valid_blks[NR_CURSEG_TYPE];

	unsigned int meta_count[META_MAX];
	unsigned int segment_count[2];
	unsigned int block_count[2];
	unsigned int inplace_count;
	unsigned long long base_mem, cache_mem, page_mem;
};

struct f2fs_inode_info {
	struct inode vfs_inode;		/* serve a vfs inode */
	unsigned long i_flags;		/* keep an inode flags for ioctl */
	unsigned char i_advise;		/* use to give file attribute hints */
	unsigned char i_dir_level;	/* use for dentry level for large dir */
	unsigned int i_current_depth;	/* only for directory depth */
	/* for gc failure statistic */
	unsigned int i_gc_failures[MAX_GC_FAILURE];
	unsigned int i_pino;		/* parent inode number */
	umode_t i_acl_mode;		/* keep file acl mode temporarily */

	/* Use below internally in f2fs*/
	unsigned long flags[BITS_TO_LONGS(FI_MAX)];	/* use to pass per-file flags */
	struct rw_semaphore i_sem;	/* protect fi info */
	atomic_t dirty_pages;		/* # of dirty pages */
	f2fs_hash_t chash;		/* hash value of given file name */
	unsigned int clevel;		/* maximum level of given file name */
	struct task_struct *task;	/* lookup and create consistency */
	struct task_struct *cp_task;	/* separate cp/wb IO stats*/
	nid_t i_xattr_nid;		/* node id that contains xattrs */
	loff_t	last_disk_size;		/* lastly written file size */
	spinlock_t i_size_lock;		/* protect last_disk_size */

#ifdef CONFIG_QUOTA
	struct dquot *i_dquot[MAXQUOTAS];

	/* quota space reservation, managed internally by quota code */
	qsize_t i_reserved_quota;
#endif
	struct list_head dirty_list;	/* dirty list for dirs and files */
	struct list_head gdirty_list;	/* linked in global dirty list */
	struct list_head inmem_ilist;	/* list for inmem inodes */
	struct list_head inmem_pages;	/* inmemory pages managed by f2fs */
	struct task_struct *inmem_task;	/* store inmemory task */
	struct mutex inmem_lock;	/* lock for inmemory pages */
	struct extent_tree *extent_tree;	/* cached extent_tree entry */

	/* avoid racing between foreground op and gc */
	struct rw_semaphore i_gc_rwsem[2];
	struct rw_semaphore i_mmap_sem;
	struct rw_semaphore i_xattr_sem; /* avoid racing between reading and changing EAs */

	int i_extra_isize;		/* size of extra space located in i_addr */
	kprojid_t i_projid;		/* id for project quota */
	int i_inline_xattr_size;	/* inline xattr size */
	struct timespec64 i_crtime;	/* inode creation time */
	struct timespec64 i_disk_time[4];/* inode disk times */

	/* for file compress */
	atomic_t i_compr_blocks;		/* # of compressed blocks */
	unsigned char i_compress_algorithm;	/* algorithm type */
	unsigned char i_log_cluster_size;	/* log of cluster size */
	unsigned char i_compress_level;		/* compress level (lz4hc,zstd) */
	unsigned short i_compress_flag;		/* compress flag */
	unsigned int i_cluster_size;		/* cluster size */
};

struct f2fs_extent {
	__le32 fofs;		/* start file offset of the extent */
	__le32 blk;		/* start block address of the extent */
	__le32 len;		/* length of the extent */
} __packed;

struct f2fs_inode {
	__le16 i_mode;			/* file mode */
	__u8 i_advise;			/* file hints */
	__u8 i_inline;			/* file inline flags */
	__le32 i_uid;			/* user ID */
	__le32 i_gid;			/* group ID */
	__le32 i_links;			/* links count */
	__le64 i_size;			/* file size in bytes */
	__le64 i_blocks;		/* file size in blocks */
	__le64 i_atime;			/* access time */
	__le64 i_ctime;			/* change time */
	__le64 i_mtime;			/* modification time */
	__le32 i_atime_nsec;		/* access time in nano scale */
	__le32 i_ctime_nsec;		/* change time in nano scale */
	__le32 i_mtime_nsec;		/* modification time in nano scale */
	__le32 i_generation;		/* file version (for NFS) */
	union {
		__le32 i_current_depth;	/* only for directory depth */
		__le16 i_gc_failures;	/*
								 * # of gc failures on pinned file.
								 * only for regular files.
								 */
	};
	__le32 i_xattr_nid;		/* nid to save xattr */
	__le32 i_flags;			/* file attributes */
	__le32 i_pino;			/* parent inode number */
	__le32 i_namelen;		/* file name length */
	__u8 i_name[F2FS_NAME_LEN];	/* file name for SPOR */
	__u8 i_dir_level;		/* dentry_level for large dir */

	struct f2fs_extent i_ext;	/* caching a largest extent */

	union {
		struct {
			__le16 i_extra_isize;	/* extra inode attribute size */
			__le16 i_inline_xattr_size;	/* inline xattr size, unit: 4 bytes */
			__le32 i_projid;	/* project id */
			__le32 i_inode_checksum;/* inode meta checksum */
			__le64 i_crtime;	/* creation time */
			__le32 i_crtime_nsec;	/* creation time in nano scale */
			__le64 i_compr_blocks;	/* # of compressed blocks */
			__u8 i_compress_algorithm;	/* compress algorithm */
			__u8 i_log_cluster_size;	/* log of cluster size */
			__le16 i_compress_flag;		/* compress flag */
			/* 0 bit: chksum flag
			 * [10,15] bits: compress level
			 */
			__le32 i_extra_end[0];	/* for attribute size calculation */
		} __packed;
		__le32 i_addr[DEF_ADDRS_PER_INODE];	/* Pointers to data blocks */
	};
	__le32 i_nid[DEF_NIDS_PER_INODE];	/* direct(2), indirect(2),
										   double_indirect(1) node id */
} __packed;

struct direct_node {
	__le32 addr[DEF_ADDRS_PER_BLOCK];	/* array of data block address */
} __packed;

struct indirect_node {
	__le32 nid[NIDS_PER_BLOCK];	/* array of data block address */
} __packed;

struct node_footer {
	__le32 nid;		/* node id */
	__le32 ino;		/* inode number */
	__le32 flag;		/* include cold/fsync/dentry marks and offset */
	__le64 cp_ver;		/* checkpoint version */
	__le32 next_blkaddr;	/* next node page block address */
} __packed;

struct f2fs_node {
	/* can be one of three types: inode, direct, and indirect types */
	union {
		struct f2fs_inode i;
		struct direct_node dn;
		struct indirect_node in;
	};
	struct node_footer footer;
} __packed;

enum {
	COLD_BIT_SHIFT = 0,
	FSYNC_BIT_SHIFT,
	DENT_BIT_SHIFT,
	OFFSET_BIT_SHIFT
};

/* Type Func */
static inline struct address_space *NODE_MAPPING(struct f2fs_sb_info *sbi)
{
	return sbi->node_inode->i_mapping;
}

static inline struct f2fs_node *F2FS_NODE(struct page *page)
{
	return (struct f2fs_node *)page_address(page);

}

static inline unsigned int ofs_of_node(struct page *node_page)
{
	struct f2fs_node *rn = F2FS_NODE(node_page);
	u32 flag = le32_to_cpu(rn->footer.flag);

	return flag >> OFFSET_BIT_SHIFT;
}

#define XATTR_NODE_OFFSET	((((unsigned int)-1) << OFFSET_BIT_SHIFT) \
		>> OFFSET_BIT_SHIFT)

static inline bool f2fs_has_xattr_block(unsigned int ofs)
{
	return ofs == XATTR_NODE_OFFSET;
}

static inline bool IS_DNODE(struct page *node_page)
{
	unsigned int ofs = ofs_of_node(node_page);

	if (f2fs_has_xattr_block(ofs))
		return true;

	if (ofs == 3 || ofs == 4 + NIDS_PER_BLOCK ||
			ofs == 5 + 2 * NIDS_PER_BLOCK)
		return false;
	if (ofs >= 6 + 2 * NIDS_PER_BLOCK) {
		ofs -= 6 + 2 * NIDS_PER_BLOCK;
		if (!((long int)ofs % (NIDS_PER_BLOCK + 1)))
			return false;
	}
	return true;
}

static inline int is_node(struct page *page, int type)
{
	struct f2fs_node *rn = F2FS_NODE(page);

	return le32_to_cpu(rn->footer.flag) & (1 << type);
}

static inline struct f2fs_inode_info *F2FS_I(struct inode *inode)
{
	return container_of(inode, struct f2fs_inode_info, vfs_inode);
}

static inline int is_file(struct inode *inode, int type)
{
	return F2FS_I(inode)->i_advise & type;
}

static inline int is_inode_flag_set(struct inode *inode, int flag)
{
	return test_bit(flag, F2FS_I(inode)->flags);
}

static inline bool f2fs_is_atomic_file(struct inode *inode)
{
	return is_inode_flag_set(inode, FI_ATOMIC_FILE);
}

static inline bool f2fs_is_volatile_file(struct inode *inode)
{
	return is_inode_flag_set(inode, FI_VOLATILE_FILE);
}

static inline struct f2fs_sb_info *F2FS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct f2fs_sb_info *F2FS_I_SB(struct inode *inode)
{
	return F2FS_SB(inode->i_sb);
}

#define PAGE_PRIVATE_GET_FUNC(name, flagname) \
	static inline bool page_private_##name(struct page *page) \
{ \
	return PagePrivate(page) && \
	test_bit(PAGE_PRIVATE_NOT_POINTER, &page_private(page)) && \
	test_bit(PAGE_PRIVATE_##flagname, &page_private(page)); \
}
PAGE_PRIVATE_GET_FUNC(gcing, ONGOING_MIGRATION);

static inline struct address_space *META_MAPPING(struct f2fs_sb_info *sbi)
{
	return sbi->meta_inode->i_mapping;
}

static inline int f2fs_compressed_file(struct inode *inode)
{
	return S_ISREG(inode->i_mode) &&
		is_inode_flag_set(inode, FI_COMPRESSED_FILE);
}

static inline bool f2fs_need_compress_data(struct inode *inode)
{
	int compress_mode = F2FS_OPTION(F2FS_I_SB(inode)).compress_mode;

	if (!f2fs_compressed_file(inode))
		return false;

	if (compress_mode == COMPR_MODE_FS)
		return true;
	else if (compress_mode == COMPR_MODE_USER &&
			is_inode_flag_set(inode, FI_ENABLE_COMPRESS))
		return true;

	return false;
}

