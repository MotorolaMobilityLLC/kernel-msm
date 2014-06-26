#ifndef __INTEL_BASINCOVE_OCD_H__
#define __INTEL_BASINCOVE_OCD_H__

#define DRIVER_NAME "bcove_bcu"
#define DEVICE_NAME "mrfl_pmic_bcu"

/* Generic bit representaion macros */
#define B0	(1 << 0)
#define B1	(1 << 1)
#define B2	(1 << 2)
#define B3	(1 << 3)
#define B4	(1 << 4)
#define B5	(1 << 5)
#define B6	(1 << 6)
#define B7	(1 << 7)

/* 30 seconds delay macro for VWARN1 interrupt Unmask (enable) */
#define VWARN2_INTR_EN_DELAY	(30 * HZ)

/* IRQ registers */
#define BCUIRQ                  0x05
#define IRQLVL1                 0x01
#define MIRQLVL1                0x0C

/* Status registers */
#define S_BCUINT                0x3B
#define S_BCUCTRL               0x49

/* PMIC SRAM address for BCU register */
#define PMIC_SRAM_BCU_ADDR      0xFFFFF614
#define IOMAP_LEN               1

#define NUM_VOLT_LEVELS         3
#define NUM_CURR_LEVELS         2

#define VWARN_EN_MASK		B3
#define ICCMAXVCC_EN_MASK	B6

#define MVWARN1_MASK		B0
#define MVWARN2_MASK		B1
#define MVCRIT_MASK		B2

#define MVCRIT			B2
#define MVWARN2			B1
#define MVWARN1			B0

#define ICCMAXVCC_EN		(1 << 6)
#define VWARN_EN		(1 << 3)
#define VCRIT_SHUTDOWN		(1 << 4)

#define BCU_ALERT               (1 << 3)
#define VWARN1_IRQ              (1 << 0)
#define VWARN2_IRQ              (1 << 1)
#define VCRIT_IRQ               (1 << 2)
#define GSMPULSE_IRQ            (1 << 3)
#define TXPWRTH_IRQ             (1 << 4)

/* Number of configurable thresholds for current and voltage */
#define NUM_THRESHOLDS          8

/* BCU real time status flags for corresponding input signals */
#define SVWARN1                 (1<<0)
#define SVWARN2                 (1<<1)
#define SVCRIT                  (1<<2)

/* S_BCUCTRL register status bits */
#define S_CAMFLTORCH		B3
#define S_CAMFLDIS		B2
#define S_BCUDISW2		B1

#define S_BCUDISW2_MASK		B1
#define S_CAMFLDIS_MASK		B2
#define S_CAMFLTORCH_MASK	B3

/* check whether bit is sticky or not by checking 5th bit */
#define IS_STICKY(data)         (!!(data & 0x10))

/* check whether signal asserted for VW1/VW2/VC */
#define IS_ASSRT_ON_VW1(data)   (!!(data & 0x01))
#define IS_ASSRT_ON_VW2(data)   (!!(data & 0x02))
#define IS_ASSRT_ON_VC(data)    (!!(data & 0x04))

/* Configuration registers that monitor the voltage drop */
#define VWARN1_CFG              0x3C
#define VWARN2_CFG              0x3D
#define VCRIT_CFG               0x3E
#define ICCMAXVSYS_CFG          0x3F
#define ICCMAXVCC_CFG           0x40
#define ICCMAXVNN_CFG           0x41

/* Behaviour registers */
#define VFLEXSRC_BEH            0x42
#define VFLEXDIS_BEH            0x43
#define VIBDIS_BEH              0x44
#define CAMFLTORCH_BEH          0x45
#define CAMFLDIS_BEH            0x46
#define BCUDISW2_BEH            0x47
#define BCUDISCRIT_BEH          0x48

/*IRQ Mask Register*/
#define MBCUIRQ                 0x10

#define MRFL_SMIP_SRAM_ADDR	0xFFFCE000
#define MOFD_SMIP_SRAM_ADDR	0xFFFC5C00

/* SMIP offset address from where the BCU related info should be read */
#define BCU_SMIP_OFFSET		0x3BA

/* No of Bytes we have to read from SMIP from BCU_SMIP_BASE*/
#define NUM_SMIP_BYTES          14

/* Max length of the register name string */
#define MAX_REGNAME_LEN		15

/* String to send the uevent along with env info to user space */
#define EVT_STR	"BCUEVT="

/* Macro to get the mode of acess for the BCU registers	*/
#define MODE(m)	(((m != S_BCUINT) && (m != BCUIRQ) && (m != IRQLVL1))	\
			? (S_IRUGO | S_IWUSR) : S_IRUGO)

/* Generic macro to assign the parameters (reg name and address) */
#define reg_info(x)	{ .name = #x, .addr = x, .mode = MODE(x) }

/* Generic macro to fill the environmental data for bcu uevent */
#define get_envp(evt)	(EVT_STR#evt)

/*
* These values are read from SMIP.
* SMIP contains these entries - default register configurations
* BCU is programmed to these default values during boot time.
*/

struct ocd_bcove_config_data {
	uint8_t vwarn1_cfg;
	uint8_t vwarn2_cfg;
	uint8_t vcrit_cfg;
	uint8_t iccmaxvsys_cfg;
	uint8_t iccmaxvcc_cfg;
	uint8_t iccmaxvnn_cfg;
	uint8_t vflexsrc_beh;
	uint8_t vflexdis_beh;
	uint8_t vibdis_beh;
	uint8_t camfltorch_beh;
	uint8_t camfldis_beh;
	uint8_t bcudisw2_beh;
	uint8_t bcudiscrit_beh;
	uint8_t mbcuirq;
} __packed;

struct ocd_platform_data {
	int (*bcu_config_data) (struct ocd_bcove_config_data *);
};

struct bcu_reg_info {
	char	name[MAX_REGNAME_LEN];	/* register name   */
	u16	addr;			/* offset address  */
	u16	mode;			/* permission mode */
};

#endif

