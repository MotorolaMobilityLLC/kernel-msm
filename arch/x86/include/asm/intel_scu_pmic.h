#ifndef __INTEL_SCU_PMIC_H__
#define __INTEL_SCU_PMIC_H__

#include <asm/types.h>

#define KOBJ_PMIC_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute _name##_attr = __ATTR(_name, _mode, _show, _store)

extern int intel_scu_ipc_ioread8(u16 addr, u8 *data);
extern int intel_scu_ipc_ioread32(u16 addr, u32 *data);
extern int intel_scu_ipc_readv(u16 *addr, u8 *data, int len);
extern int intel_scu_ipc_iowrite8(u16 addr, u8 data);
extern int intel_scu_ipc_writev(u16 *addr, u8 *data, int len);
extern int intel_scu_ipc_update_register(u16 addr, u8 data, u8 mask);

#endif /*__INTEL_SCU_PMIC_H__ */
