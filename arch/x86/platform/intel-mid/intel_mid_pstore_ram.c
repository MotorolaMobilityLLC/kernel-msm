/*
 * Intel mid pstore ram support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/platform_device.h>
#include <linux/memblock.h>
#include <linux/pstore_ram.h>
#include <linux/bootmem.h>
#include <linux/efi.h>
#include <linux/nls.h>

#define SZ_4K	0x00001000
#define SZ_2M	0x00200000
#define SZ_2_1M	0x00219000
#define SZ_16M	0x01000000

/* Board files use the following if they are ok with 16M start defaults */
#define PSTORE_RAM_START_DEFAULT	SZ_16M
#define PSTORE_RAM_SIZE_DEFAULT		SZ_2_1M

#ifdef CONFIG_X86_32
#define RAM_MAX_MEM (max_low_pfn << PAGE_SHIFT)
#else
#define RAM_MAX_MEM (1 << 28)
#endif

static struct ramoops_platform_data pstore_ram_data = {
	.mem_size	= PSTORE_RAM_SIZE_DEFAULT,
	.mem_address	= PSTORE_RAM_START_DEFAULT,
	.record_size	= SZ_4K,
	.console_size	= SZ_2M,
	.ftrace_size	= 2*SZ_4K,
	.dump_oops	= 1,
};

static struct platform_device pstore_ram_dev = {
	.name = "ramoops",
	.dev = {
		.platform_data = &pstore_ram_data,
	},
};

static __initdata bool intel_mid_pstore_ram_inited;

static const char EFIVAR_PSTORE_ADDR[] = "PstoreAddr";
static const char EFIVAR_PSTORE_SIZE[] = "PstoreSize";

static void uefi_set_pstore_buffer(unsigned long *addr, unsigned long *size)
{
	int ret;
	wchar_t varname[sizeof(EFIVAR_PSTORE_ADDR)];
	u32 attributes = EFI_VARIABLE_NON_VOLATILE
		| EFI_VARIABLE_BOOTSERVICE_ACCESS
		| EFI_VARIABLE_RUNTIME_ACCESS;

	utf8s_to_utf16s(EFIVAR_PSTORE_ADDR, sizeof(EFIVAR_PSTORE_ADDR),
			UTF16_LITTLE_ENDIAN, varname, sizeof(varname));
	varname[sizeof(EFIVAR_PSTORE_ADDR) - 1] = 0;

	ret = efivar_entry_set_safe(varname, EFI_GLOBAL_VARIABLE_GUID,
				    attributes, true,
				    sizeof(unsigned long), addr);
	if (ret) {
		pr_err("%s can't set variable %s (%d)\n",
		       __func__, EFIVAR_PSTORE_ADDR, ret);
		return;
	}

	utf8s_to_utf16s(EFIVAR_PSTORE_SIZE, sizeof(EFIVAR_PSTORE_SIZE),
			UTF16_LITTLE_ENDIAN, varname, sizeof(varname));
	varname[sizeof(EFIVAR_PSTORE_SIZE) - 1] = 0;

	ret = efivar_entry_set_safe(varname, EFI_GLOBAL_VARIABLE_GUID,
				    attributes, true,
				    sizeof(unsigned long), size);
	if (ret)
		pr_err("%s can't set variable %s (%d)\n",
		       __func__, EFIVAR_PSTORE_SIZE, ret);
}

/**
 * intel_mid_pstore_ram_register() - device_initcall to register ramoops device
 */
static int __init intel_mid_pstore_ram_register(void)
{
	int ret;

	if (!intel_mid_pstore_ram_inited)
		return -ENODEV;

	ret = platform_device_register(&pstore_ram_dev);
	if (ret) {
		pr_err("%s: unable to register pstore_ram device: "
		       "start=0x%llx, size=0x%lx, ret=%d\n", __func__,
		       (unsigned long long)pstore_ram_data.mem_address,
		       pstore_ram_data.mem_size, ret);
	}

	if (efi_enabled(EFI_BOOT) && efi_enabled(EFI_RUNTIME_SERVICES))
		uefi_set_pstore_buffer(&pstore_ram_data.mem_address,
				       &pstore_ram_data.mem_size);

	return ret;
}
device_initcall(intel_mid_pstore_ram_register);

void __init pstore_ram_reserve_memory(void)
{
	phys_addr_t mem;
	size_t size;
	int ret;

	size = PSTORE_RAM_SIZE_DEFAULT;
	size = ALIGN(size, PAGE_SIZE);

	mem = memblock_find_in_range(0, RAM_MAX_MEM, size, PAGE_SIZE);
	if (!mem) {
		pr_err("Cannot find memblock range for pstore_ram\n");
		return;
	}

	ret = memblock_reserve(mem, size);
	if (ret) {
		pr_err("Failed to reserve memory from 0x%llx-0x%llx\n",
		       (unsigned long long)mem,
		       (unsigned long long)(mem + size - 1));
		return;
	}

	pstore_ram_data.mem_address = mem;
	pstore_ram_data.mem_size = size;

	pr_info("reserved RAM buffer (0x%zx@0x%llx)\n",
		size, (unsigned long long)mem);

	intel_mid_pstore_ram_inited = true;
}
