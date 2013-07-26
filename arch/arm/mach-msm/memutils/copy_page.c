#include <linux/string.h>
#include <asm/page.h>
void copy_page(void *to, const void *from)
{
	memcpy(to, from, PAGE_SIZE);
}
