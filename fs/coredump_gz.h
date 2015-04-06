#ifndef _FS_COREDUMP_GZ_H
#define _FS_COREDUMP_GZ_H

#include <linux/module.h>
#include <linux/binfmts.h>

#ifdef CONFIG_COREDUMP_GZ
static inline int dump_compressed(struct coredump_params *cprm)
{
	return cprm->gz;
}

void check_for_gz(int ispipe, const char *cp, struct coredump_params *cprm);

int gz_init(struct coredump_params *cprm);
int gz_dump_write(struct coredump_params *cprm, const void *addr, int nr);
int gz_finish(struct coredump_params *cprm);
#else
static inline int dump_compressed(struct coredump_params *cprm) { return 0; }

static void check_for_gz(int ispipe, const char *cp,
		struct coredump_params *cprm) { return; }

static inline int gz_init(struct coredump_params *cprm) { return 1; }
static inline int gz_dump_write(struct coredump_params *cprm, const void *addr,
		int nr) { return 1; }
static inline int gz_finish(struct coredump_params *cprm) { return 1; }
#endif


#endif /* _FS_COREDUMP_GZ_H */
