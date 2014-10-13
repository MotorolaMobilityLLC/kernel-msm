
#ifndef M4SENSORHUB_STM32_BL_CMDS_H
#define M4SENSORHUB_STM32_BL_CMDS_H

#ifdef __KERNEL__

#define ACK     ((uint8_t) (0x79))
#define NACK    ((uint8_t) (0x1F))
#define BUSY    ((uint8_t) (0x76))

/* LIST OF OPCODES FOR BOOTLOADER */
/* opcode_get */
#define OPC_GET ((uint8_t) (0x00))
/* opcode_get_version */
#define OPC_GV  ((uint8_t) (0x01))
/* opcode_get_id */
#define OPC_GID ((uint8_t) (0x02))
/* opcode_read_memory */
#define OPC_RM  ((uint8_t) (0x11))
/* opcode_go */
#define OPC_GO  ((uint8_t) (0x21))
/* opcode_write_memory */
#define OPC_WM  ((uint8_t) (0x31))
/* opcode_no_stretch_write_memory */
#define OPC_NO_STRETCH_WM   ((uint8_t) (0x32))
/* opcode_erase */
#define OPC_ER  ((uint8_t) (0x44))
/* opcode_no_stretch_erase */
#define OPC_NO_STRETCH_ER   ((uint8_t) (0x45))
/* opcode_write_protect */
#define OPC_WP  ((uint8_t) (0x63))
/* opcode_write_unprotect */
#define OPC_WU  ((uint8_t) (0x73))
/* opcode_readout_protect */
#define OPC_RP  ((uint8_t) (0x82))
/* opcode_readout_unprotect */
#define OPC_RU  ((uint8_t) (0x92))
/* opcode_no_stretch_readout_unprotect */
#define OPC_NO_STRETCH_RU   ((uint8_t) (0x93))


int m4sensorhub_bl_ack(struct m4sensorhub_data *m4sensorhub);
int m4sensorhub_bl_rm(struct m4sensorhub_data *m4sensorhub,
	int start_address, u8 *data, int len);
int m4sensorhub_bl_go(struct m4sensorhub_data *m4sensorhub,
	int start_address);
int m4sensorhub_bl_wm(struct m4sensorhub_data *m4sensorhub,
	int start_address, u8 *data, int len);
int m4sensorhub_bl_erase_sectors(struct m4sensorhub_data *m4sensorhub,
	int *sector_array, int numsectors);

#endif /* __KERNEL__ */
#endif /* M4SENSORHUB_STM32_BL_CMDS_H */
