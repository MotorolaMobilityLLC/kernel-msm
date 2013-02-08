#define pr_fmt(fmt) "module-whitelist: " fmt

#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/scatterlist.h>
#include <linux/string.h>

#include "module-whitelist.h"

#define MODULE_WHITELIST_HASH_ALG	"sha1"

struct whitelist_entry {
	struct list_head list;
	u8 digest[];
};

static DEFINE_RWLOCK(whitelist_rwlock);
static LIST_HEAD(module_whitelist);
static bool list_locked;

static unsigned int module_whitelist_digest_len(void)
{
	struct crypto_hash *tfm;
	static unsigned int digest_len;

	if (digest_len)
		return digest_len;

	tfm = crypto_alloc_hash(MODULE_WHITELIST_HASH_ALG, 0,
			CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		pr_err("Failed to load cryptographic transform: %s\n",
				MODULE_WHITELIST_HASH_ALG);
		return 0;
	}
	digest_len = crypto_hash_digestsize(tfm);
	crypto_free_hash(tfm);

	return digest_len;
}

static void report_bad_hash(const u8 *digest, unsigned int len)
{
	char *ascii;
	char *buf;
	int i;

	buf = ascii = kzalloc(len * 2 + 1, GFP_KERNEL);
	if (!ascii)
		return;

	for (i = 0; i < len; i++) {
		snprintf(buf, 3, "%02x", digest[i]);
		buf += 2;
	}
	pr_err("Module hash not found in whitelist: %s\n", ascii);
	kfree(ascii);
}

int check_module_hash(const Elf_Ehdr *hdr, unsigned long len)
{
	int rc;
	struct hash_desc desc;
	u8 *digest;
	unsigned int digest_len;
	struct whitelist_entry *entry;
	struct scatterlist *sg = NULL;
	struct scatterlist *ptr = NULL;
	unsigned long size;
	const u8 *data;

	if (0 >= len)
		return -EINVAL;

	digest_len = module_whitelist_digest_len();
	if (!digest_len)
		return -EIO;

	desc.tfm = crypto_alloc_hash(MODULE_WHITELIST_HASH_ALG, 0,
			CRYPTO_ALG_ASYNC);
	if (IS_ERR(desc.tfm)) {
		pr_err("Failed to load cryptographic transform: %s\n",
				MODULE_WHITELIST_HASH_ALG);
		return -ENODEV;
	}

	desc.flags = 0;
	rc = crypto_hash_init(&desc);
	if (rc) {
		pr_err("%s: Error initializing crypto hash; rc = [%d]\n",
				__func__, rc);
		goto free_hash;
	}

	size = len;
	data = (const u8 *)hdr;
	while (size) {
		struct page *pg = vmalloc_to_page(data);
		unsigned long offset = offset_in_page(data);
		unsigned long size_in_page = min(size,
				((unsigned int)(PAGE_SIZE)) - offset);

		if (!pg) {
			rc = -EFAULT;
			goto free_hash;
		}

		/* on the first iteration through */
		if (!sg) {
			unsigned long count = len / PAGE_SIZE;

			/* is there a fractional page at the beginning? */
			if (offset)
				count++;

			/* how about a fractional page at the end? */
			if ((size - size_in_page) % PAGE_SIZE)
				count++;

			ptr = sg = kmalloc(sizeof(struct scatterlist) * count,
					GFP_KERNEL);
			if (!sg) {
				rc = -ENOMEM;
				goto free_hash;
			}
			sg_init_table(sg, count);
		}

		sg_set_page(ptr++, pg, size_in_page, offset);

		size -= size_in_page;
		data += size_in_page;
	}

	digest = kmalloc(digest_len, GFP_KERNEL);
	if (!digest) {
		rc = -ENOMEM;
		goto free_sg;
	}

	rc = crypto_hash_digest(&desc, sg, len, digest);
	if (rc) {
		pr_err("%s: Error generating crypto digest; rc = [%d]\n",
				__func__, rc);
		goto free_digest;
	}

	read_lock(&whitelist_rwlock);

	rc = -EPERM;
	list_for_each_entry(entry, &module_whitelist, list) {
		if (memcmp(digest, entry->digest, digest_len) == 0) {
			pr_debug("Computed digest matched in whitelist.\n");
			rc = 0;
			break;
		}
	}

	read_unlock(&whitelist_rwlock);

	if (-EPERM == rc) /* do this outside of the lock */
		report_bad_hash(digest, digest_len);

free_digest:
	kfree(digest);

free_sg:
	kfree(sg);

free_hash:
	crypto_free_hash(desc.tfm);

	return rc;
}

static ssize_t module_whitelist_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct whitelist_entry *entry;
	ssize_t result, count = PAGE_SIZE;
	unsigned int digest_len;
	int i;

	digest_len = module_whitelist_digest_len();
	if (!digest_len)
		return -EIO;

	read_lock(&whitelist_rwlock);

	list_for_each_entry(entry, &module_whitelist, list) {
		for (i = 0; i < digest_len; i++) {
			result = scnprintf(buf, count, "%02x",
					entry->digest[i]);
			if (result > 0) {
				count -= result;
				buf += result;
			}
		}

		result = scnprintf(buf, count, "\n");
		if (result > 0) {
			count -= result;
			buf += result;
		}
	}

	read_unlock(&whitelist_rwlock);

	return PAGE_SIZE - count;
}

static ssize_t module_whitelist_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rc;
	size_t buf_idx, digest_idx;
	unsigned int digest_len;
	char *ascii;
	struct list_head *pos, *q;

	if ((count > PAGE_SIZE) || (count == 0))
		return -EINVAL;

	digest_len = module_whitelist_digest_len();
	if (!digest_len)
		return -EIO;

	ascii = kzalloc(digest_len * 2 + 1, GFP_ATOMIC);
	if (!ascii)
		return -ENOMEM;

	write_lock(&whitelist_rwlock);

	/* only allow one write of the module whitelist */
	if (list_locked) {
		rc = -EPERM;
		goto err_out;
	}

	list_locked = true;

	BUG_ON(!list_empty(&module_whitelist));

	for (buf_idx = 0, digest_idx = 0; buf_idx < count; buf_idx++) {
		/* skip blank lines */
		if ((0 == digest_idx) && ((buf[buf_idx] == '\r')
					|| (buf[buf_idx] == '\n')))
			continue;

		if (digest_idx < digest_len * 2) {
			/* skip leading whitespace */
			if (buf[buf_idx] == ' ')
				continue;

			if (!isxdigit(buf[buf_idx])) {
				pr_debug("not an x digit: 0x%02d\n",
						buf[buf_idx]);
				rc = -EINVAL;
				goto err_clearlist;
			}

			ascii[digest_idx] = buf[buf_idx];
		} else if (digest_idx == digest_len * 2) {
			/* ensure digest is actually the right length */
			if ((buf[buf_idx] != ' ') && (buf[buf_idx] != '\n')
					&& (buf[buf_idx] != '\r')) {
				pr_debug("found digest of unexpected length\n");
				rc = -EINVAL;
				goto err_clearlist;
			}
		}

		/* ignore everything after the digest itself */

		if ((buf[buf_idx] == '\r') || (buf[buf_idx] == '\n')) {
			struct whitelist_entry *entry;

			entry = kzalloc(sizeof(struct whitelist_entry)
					+ digest_len, GFP_ATOMIC);
			if (!entry) {
				pr_debug("can't allocate white list entry\n");
				rc = -ENOMEM;
				goto err_clearlist;
			}

			rc = hex2bin(entry->digest, ascii, digest_len);
			if (rc) {
				kfree(entry);
				rc = -EINVAL; /* hex2bin returns -1 */
				goto err_clearlist;
			}

			pr_debug("added hash entry: %s\n", ascii);
			list_add(&entry->list, &module_whitelist);

			digest_idx = 0;
		} else
			digest_idx++;
	}

	/* ensure we didn't hit an unexpected EOF */
	if ((digest_idx > 0) && (digest_idx < digest_len * 2)) {
		pr_debug("unexpected EOF\n");
		rc = -EINVAL;
		goto err_clearlist;
	}

	write_unlock(&whitelist_rwlock);

	kfree(ascii);

	return buf_idx;

err_clearlist:
	list_for_each_safe(pos, q, &module_whitelist) {
		struct whitelist_entry *entry = list_entry(pos,
				struct whitelist_entry, list);
		list_del(pos);
		kfree(entry);
	}

err_out:
	write_unlock(&whitelist_rwlock);

	kfree(ascii);

	return rc;
}

static struct kobj_attribute module_whitelist_attr = __ATTR(module_whitelist,
		S_IRUGO | S_IWUSR, module_whitelist_show,
		module_whitelist_store);

static int __init module_whitelist_init(void)
{
	int rc = sysfs_create_file(kernel_kobj, &module_whitelist_attr.attr);
	if (rc)
		pr_err("Failed to creates module_whitelist sysfs entry.\n");

	return rc;
}
module_init(module_whitelist_init);
