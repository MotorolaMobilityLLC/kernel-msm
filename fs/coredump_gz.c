#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>

#include "internal.h"
#include "coredump_gz.h"

#define  OUT_BUF_SIZE  (100*1024)

void check_for_gz(int ispipe, const char *cp, struct coredump_params *cprm)
{
	if (!ispipe) {
		char *str = strrchr(cp, '.');

		cprm->gz = str && !strcmp(str, ".gz");
	} else {
		cprm->gz = 0;
	}
}

int gz_init(struct coredump_params *cprm)
{
	unsigned char gz_magic[10] = { /* gzip magic header */
		0x1f, 0x8b, Z_DEFLATED, 0, 0, 0, 0, 0, 0, 0x03 };

	cprm->crc = ~0; /* init */

	cprm->out_buf = vmalloc(OUT_BUF_SIZE);
	if (!cprm->out_buf) {
		pr_warn("Failed to allocate deflate buffer");
		return 0;
	}

	cprm->deflate_workspace = vmalloc(zlib_deflate_workspacesize(-15, 6));
	if (!cprm->deflate_workspace) {
		vfree(cprm->out_buf);
		pr_warn("Failed to allocate deflate workspace\n");
		return 0;
	}

	memset(&cprm->zstr, 0, sizeof(z_stream));
	cprm->zstr.workspace = cprm->deflate_workspace;
	if (Z_OK != zlib_deflateInit2(&cprm->zstr,
				7, /* compression level */
				Z_DEFLATED,
				-15, /* window bits */
				6, /* mem level */
				Z_DEFAULT_STRATEGY)) {
		pr_warn("deflateInit failed\n");
		vfree(cprm->deflate_workspace);
		vfree(cprm->out_buf);
		return 0;
	}

	return __dump_emit(cprm, gz_magic, 10);
}

int gz_dump_write(struct coredump_params *cprm, const void *addr, int nr)
{
	int all_fine = 1;

	cprm->crc = crc32_le(cprm->crc, addr, nr);

	cprm->zstr.next_in = (void *)addr;
	cprm->zstr.avail_in = nr;

	do {
		cprm->zstr.next_out = cprm->out_buf;
		cprm->zstr.avail_out = OUT_BUF_SIZE;

		if (zlib_deflate(&cprm->zstr, Z_NO_FLUSH) == Z_OK) {
			/* new output generated */
			all_fine = __dump_emit(cprm, cprm->out_buf,
					OUT_BUF_SIZE - cprm->zstr.avail_out);
		}
	} while ((cprm->zstr.avail_out != OUT_BUF_SIZE) && all_fine);

	return all_fine;
}

int gz_finish(struct coredump_params *cprm)
{
	unsigned char gz_footer[8];
	int all_fine = 1;
	int ret;

	cprm->zstr.next_in = 0;
	cprm->zstr.avail_in = 0;

	do {
		cprm->zstr.next_out  = cprm->out_buf;
		cprm->zstr.avail_out = OUT_BUF_SIZE;

		ret = zlib_deflate(&cprm->zstr, Z_FINISH);
		if ((ret == Z_OK) || (ret == Z_STREAM_END)) {
			/* new output generated */
			all_fine = __dump_emit(cprm, cprm->out_buf,
					OUT_BUF_SIZE - cprm->zstr.avail_out);
		}
	} while ((cprm->zstr.avail_out != OUT_BUF_SIZE)
			&& all_fine && (ret != Z_STREAM_END));

	zlib_deflateEnd(&cprm->zstr);

	if (!all_fine)
		goto out;

	gz_footer[0] = (~cprm->crc & 0x000000FF);
	gz_footer[1] = (~cprm->crc & 0x0000FF00) >> 8;
	gz_footer[2] = (~cprm->crc & 0x00FF0000) >> 16;
	gz_footer[3] = (~cprm->crc & 0xFF000000) >> 24;
	gz_footer[4] = (cprm->zstr.total_in & 0x000000FF);
	gz_footer[5] = (cprm->zstr.total_in & 0x0000FF00) >> 8;
	gz_footer[6] = (cprm->zstr.total_in & 0x00FF0000) >> 16;
	gz_footer[7] = (cprm->zstr.total_in & 0xFF000000) >> 24;

	all_fine =  __dump_emit(cprm, gz_footer, 8);

out:
	vfree(cprm->deflate_workspace);
	vfree(cprm->out_buf);

	return all_fine;
}
