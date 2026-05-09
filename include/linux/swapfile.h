/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SWAPFILE_H
#define _LINUX_SWAPFILE_H

extern unsigned long generic_max_swapfile_size(void);
unsigned long arch_max_swapfile_size(void);

extern int unuse_swap_pte(struct vm_area_struct *vma, pmd_t *pmd,
		unsigned long addr, swp_entry_t entry, struct folio *folio);

/* Maximum swapfile size supported for the arch (not inclusive). */
extern unsigned long swapfile_maximum_size;
/* Whether swap migration entry supports storing A/D bits for the arch */
extern bool swap_migration_ad_supported;

#endif /* _LINUX_SWAPFILE_H */
