#include <linux/module.h>

#ifdef CONFIG_MODULE_WHITELIST
int check_module_hash(const Elf_Ehdr *hdr, unsigned long len);
#else
static inline int check_module_hash(const Elf_Ehdr *hdr, unsigned long len)
{
	return 0;
}
#endif
