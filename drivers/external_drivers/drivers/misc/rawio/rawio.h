#ifndef RAWIO_H
#define RAWIO_H

#define RAWIO_DRVNAME_LEN	8
#define RAWIO_ERR_LEN		256
#define RAWIO_CMD_LEN		96
#define RAWIO_HELP_LEN		128
#define RAWIO_ARGS_MIN		3
#define RAWIO_ARGS_MAX		10

enum type {
	TYPE_STR = 0,
	TYPE_U8,
	TYPE_U16,
	TYPE_U32,
	TYPE_U64,
	TYPE_S8,
	TYPE_S16,
	TYPE_S32,
	TYPE_S64,
};

/* read/write width: 1, 2, 4 or 8 bytes */
enum width {
	WIDTH_DEFAULT = 0,
	WIDTH_1 = 1,
	WIDTH_2 = 2,
	WIDTH_4 = 4,
	WIDTH_8 = 8,
};

enum ops {
	OPS_RD = 1,	/* read */
	OPS_WR,		/* write */
};

struct rawio_driver {
	struct list_head list;
	char name[RAWIO_DRVNAME_LEN];

	int args_rd_max_num; /* max args for read(including optional args) */
	enum type args_rd_types[RAWIO_ARGS_MAX]; /* type of each arg */
	int args_rd_min_num; /* min args for read */
	char args_rd_postfix[RAWIO_ARGS_MAX]; /* read args postfix */

	int args_wr_max_num; /* max args for write(including optional args) */
	enum type args_wr_types[RAWIO_ARGS_MAX]; /* type of each arg */
	int args_wr_min_num; /* min args for write */
	char args_wr_postfix[RAWIO_ARGS_MAX]; /* write args postfix */

	/* index of argument that specifies the register or memory address */
	int addr_pos;

	unsigned int supported_width;
	enum width default_width;
	char help[RAWIO_HELP_LEN];
	struct rawio_ops *ops;
	struct seq_file *s;
};

struct rawio_ops {
	/* driver reads io device and returns the data to framework */
	int (*read) (struct rawio_driver *drv, int width,
		u64 *input, u8 *postfix, int input_num,
		void **output, int *output_num);
	/* driver reads io device and shows the data */
	int (*read_and_show) (struct rawio_driver *drv, int width,
		u64 *input, u8 *postfix, int input_num);
	/* driver writes data passed from framework to io device */
	int (*write) (struct rawio_driver *driver, int width,
		u64 *input, u8 *postfix, int input_num);
};

int rawio_register_driver(struct rawio_driver *drv);
int rawio_unregister_driver(struct rawio_driver *drv);
void rawio_err(const char *fmt, ...);

#endif
