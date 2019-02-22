#include "goodix_ts_core.h"

#define MOTO_SENSOR_NUM_ADDR			0x5473
#define MOTO_DRIVER_GROUP_A_NUM_ADDR		0x5477
#define MOTO_DRIVER_GROUP_B_NUM_ADDR		0x5478
#define MOTO_READ_COOR_ADDR			0x4100
#define MOTO_RAW_DATA_BASE_ADDR			0x8FA0
#define MOTO_DIFF_DATA_BASE_ADDR		0x9D20
#define MOTO_REF_DATA_BASE_ADDR			0xA980
#define MOTO_CMD_ADDR				0x6F68

static void goodix_cmds_init(struct goodix_ts_cmd *ts_cmd,
					     u8 cmds, u8 cmd_data, u32 reg_addr)
{
	if (reg_addr) {
		ts_cmd->cmd_reg = reg_addr;
		ts_cmd->length = 3;
		ts_cmd->cmds[0] = cmds;
		ts_cmd->cmds[1] = cmd_data;
		ts_cmd->cmds[2] = 0 - cmds - cmd_data;
		ts_cmd->initialized = true;
	} else {
		ts_cmd->initialized = false;
	}
}

int goodix_get_channel_num(u32 *sen_num, u32 *drv_num)
{
	int ret = -1;
	u8 buf[2] = {0};
	struct goodix_ts_device *ts_dev = goodix_modules.core_data->ts_dev;

	ret = goodix_i2c_read(ts_dev, MOTO_SENSOR_NUM_ADDR, buf, 1);
	if (ret) {
		ts_err("Read sen_num fail.");
		return ret;
	}

	*sen_num = buf[0];

	ret = goodix_i2c_read(ts_dev, MOTO_DRIVER_GROUP_A_NUM_ADDR, buf, 2);
	if (ret) {
		ts_err("Read drv_num fail.");
		return ret;
	}

	*drv_num = buf[0] + buf[1];
	ts_info("sen_num : %d, drv_num : %d", *sen_num, *drv_num);

	return 0;
}

static int goodix_be_normal(void)
{
	int ret = -1;
	struct goodix_ts_cmd normal_cmd;
	struct goodix_ts_device *ts_dev = goodix_modules.core_data->ts_dev;

	goodix_cmds_init(&normal_cmd, 0x00, 0, ts_dev->reg.command);
	ret = ts_dev->hw_ops->send_cmd(ts_dev, &normal_cmd);
	if (ret)
		ts_err("Send enter normal mode command fail.");

	return ret;
}

int goodix_get_rawordiff_data(int which, int *data)
{
	int i, wait_cnt, ret = 0;
	u32 sen_num = 0, drv_num = 0;
	u8 *temp_data = NULL;
	s16 *result_data = NULL;
	int data_size = 0, data_len = 0;
	u8 buf[2] = {0};
	struct goodix_ts_cmd cmd;
	struct goodix_ts_device *ts_dev = goodix_modules.core_data->ts_dev;

	if ((which != 0) && (which != 1) && (which != 2) &&
		(which != 3) && (which != 4) && (which != 5)) {
		ts_err("Invalid get rawdiff data params.");
		return -EINVAL;
	}

	ret = goodix_get_channel_num(&sen_num, &drv_num);
	if (ret) {
		ts_err("Get channel num fail.");
		return -EINVAL;
	}

	if (which < 3)
		data_size = sen_num * drv_num;
	else
		data_size = sen_num + drv_num;

	data_len = data_size * sizeof(u16);
	if (data == NULL) {
		ts_err("User data format error.");
		return -EINVAL;
	}

	temp_data = kzalloc(data_len, GFP_KERNEL);
	if (!temp_data)
		return -EINVAL;

	result_data = kzalloc(data_len, GFP_KERNEL);
	if (!result_data) {
		ret = -EINVAL;
		goto free_data;
	}

	goodix_cmds_init(&cmd, 0x01, 0, ts_dev->reg.command);

	ret = ts_dev->hw_ops->send_cmd(ts_dev, &cmd);
	if (ret) {
		ts_err("Send enter rawdiff mode fail.");
		ret = -EINVAL;
		goto free_data;
	}

	usleep_range(18000, 18100);

	/*goodix_ts_irq_enable(goodix_modules.core_data, false);*/
	ret = goodix_i2c_write(ts_dev, MOTO_READ_COOR_ADDR, buf, 2);
	if (ret) {
		ts_err("Set zero to interrupt status reg fail.");
		ret = -EINVAL;
		goto be_normal;
	}

	for (wait_cnt = 0; wait_cnt < 3; wait_cnt++) {
		msleep(150);
		ret = goodix_i2c_read(ts_dev, MOTO_READ_COOR_ADDR, buf, 2);
		if ((!ret) && (buf[0] & 0x80))
			break;
	}
	/*goodix_ts_irq_enable(goodix_modules.core_data, true);*/

	if (wait_cnt < 3) {
		switch (which) {
		case 0:
			ret = goodix_i2c_read(ts_dev, MOTO_RAW_DATA_BASE_ADDR,
						temp_data, data_len);
			if (ret) {
				ts_err("Read rawdata fail.");
				ret = -EINVAL;
				goto be_normal;
			}
			break;
		case 1:
			ret = goodix_i2c_read(ts_dev, MOTO_DIFF_DATA_BASE_ADDR,
						temp_data, data_len);
			if (ret) {
				ts_err("Read diffdata fail.");
				ret = -EINVAL;
				goto be_normal;
			}
			break;
		case 2:
			ret = goodix_i2c_read(ts_dev, MOTO_REF_DATA_BASE_ADDR,
						temp_data, data_len);
			if (ret) {
				ts_err("Read ref data fail.");
				ret = -EINVAL;
				goto be_normal;
			}
			break;
		case 3:
			ret = goodix_i2c_read(ts_dev, 0x4c0c,
						temp_data, data_len);
			if (ret) {
				ts_err("Read ref data fail.");
				ret = -EINVAL;
				goto be_normal;
			}
			break;
		case 4:
			ret = goodix_i2c_read(ts_dev, 0x4ca4,
						temp_data, data_len);
			if (ret) {
				ts_err("Read ref data fail.");
				ret = -EINVAL;
				goto be_normal;
			}
			break;
		case 5:
			ret = goodix_i2c_read(ts_dev, 0x4b74,
						temp_data, data_len);
			if (ret) {
				ts_err("Read ref data fail.");
				ret = -EINVAL;
				goto be_normal;
			}
			break;
		}

		for (i = 0; i < data_size; i++) {
			result_data[i] = (temp_data[2 * i] << 8) |
					temp_data[2 * i + 1];
			data[i] = result_data[i];
		}

		buf[0] = buf[1] = 0x00;
		ret = goodix_i2c_write(ts_dev, MOTO_READ_COOR_ADDR, buf, 2);
		if (ret) {
			ts_err("Set zero to interrupt status reg fail.");
			ret = -EINVAL;
			goto be_normal;
		}
	} else {
		ts_err("Wait for data ready timeout.");
		ret = -EINVAL;
		goto be_normal;
	}

be_normal:

	goodix_be_normal();

free_data:

	kfree(temp_data);
	temp_data = NULL;
	kfree(result_data);
	result_data = NULL;

	return ret;
}

int goodix_nodereg_read(void)
{
	char tm_buf[50];
	char char_buf[100];
	int ret = 0, i, j, offset = 0;
	struct goodix_ts_device *ts_dev = goodix_modules.core_data->ts_dev;

	ret |= goodix_i2c_read(ts_dev, 0x2031, tm_buf, 1);
	ts_info("0x2031 = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x20b0, tm_buf, 1);
	ts_info("0x20b0 = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x20C0, tm_buf, 2);
	ts_info("0x20C0 = 0x%02x%02x", tm_buf[0], tm_buf[1]);
	ret |= goodix_i2c_read(ts_dev, 0x21e4, tm_buf, 1);
	ts_info("0x21e4 = 0x%02x", tm_buf[0]);

	offset = 0;
	for (i = 0; i < 10; i++) {
		ret |= goodix_i2c_read(ts_dev, 0x2244, tm_buf, 2);

		offset += snprintf(&char_buf[offset], PAGE_SIZE,
			"0x%02x", tm_buf[0]);
		offset += snprintf(&char_buf[offset], PAGE_SIZE,
			"%02x ", tm_buf[1]);
	}
	char_buf[offset] = 0;
	ts_info("0x2244 = %s", &char_buf);

	offset = 0;
	for (i = 0; i < 10; i++) {
		ret |= goodix_i2c_read(ts_dev, 0x2244, tm_buf, 2);

		offset += snprintf(&char_buf[offset], PAGE_SIZE,
			"0x%02x", tm_buf[0]);
		offset += snprintf(&char_buf[offset], PAGE_SIZE,
			"%02x ", tm_buf[1]);
	}
	char_buf[offset] = 0;
	ts_info("0x2244 = %s", &char_buf);

	ret |= goodix_i2c_read(ts_dev, 0x225f, tm_buf, 1);
	ts_info("0x225f = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x2276, tm_buf, 1);
	ts_info("0x2276 = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x2260, tm_buf, 2);
	ts_info("0x2260 = 0x%02x%02x", tm_buf[0], tm_buf[1]);
	ret |= goodix_i2c_read(ts_dev, 0x2264, tm_buf, 1);
	ts_info("0x2264 = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x2266, tm_buf, 1);
	ts_info("0x2266 = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x2268, tm_buf, 1);
	ts_info("0x2268 = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x2269, tm_buf, 1);
	ts_info("0x2269 = 0x%02x", tm_buf[0]);

	ret |= goodix_i2c_read(ts_dev, 0x228e, tm_buf, 2);
	ts_info("0x228e = 0x%02x%02x", tm_buf[0], tm_buf[1]);

	for (i = 0; i < 5; i++) {
		ret |= goodix_i2c_read(ts_dev, 0x4040, tm_buf, 1);
		ts_info("0x4040 = 0x%02x", tm_buf[0]);
	}
	for (i = 0; i < 5; i++) {
		ret |= goodix_i2c_read(ts_dev, 0x4041, tm_buf, 1);
		ts_info("0x4041 = 0x%02x", tm_buf[0]);
	}

	for (j = 0; j < 10; j++) {
		ret |= goodix_i2c_read(ts_dev, 0x4100, tm_buf, 20);
		offset = 0;
		for (i = 0; i < 20; i++) {
			offset += snprintf(&char_buf[offset], PAGE_SIZE,
				"%02x ", tm_buf[i]);
		}
		char_buf[offset] = 0;
		ts_info("0x4100 = %s", &char_buf);
	}

	ret |= goodix_i2c_read(ts_dev, 0x4535, tm_buf, 8);
	offset = 0;
	for (i = 0; i < 8; i++) {
		offset += snprintf(&char_buf[offset], PAGE_SIZE,
			"%02x ", tm_buf[i]);
	}
	char_buf[offset] = 0;
	ts_info("0x4535 = %s", &char_buf);

	ret |= goodix_i2c_read(ts_dev, 0x453d, tm_buf, 4);
	offset = 0;
	for (i = 0; i < 4; i++) {
		offset += snprintf(&char_buf[offset], PAGE_SIZE,
			"%02x ", tm_buf[i]);
	}
	char_buf[offset] = 0;
	ts_info("0x453d = %s", &char_buf);

	ret |= goodix_i2c_read(ts_dev, 0x4541, tm_buf, 1);
	ts_info("0x4541 = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x5186, tm_buf, 1);
	ts_info("0x5186 = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x518b, tm_buf, 1);
	ts_info("0x518b = 0x%02x", tm_buf[0]);

	for (i = 0; i < 5; i++) {
		ret |= goodix_i2c_read(ts_dev, 0x5193, tm_buf, 2);
		ts_info("0x5193 = 0x%02x%02x", tm_buf[0], tm_buf[1]);
	}
	for (i = 0; i < 5; i++) {
		ret |= goodix_i2c_read(ts_dev, 0x5195, tm_buf, 1);
		ts_info("0x5195 = 0x%02x", tm_buf[0]);
	}

	ret |= goodix_i2c_read(ts_dev, 0x52cc, tm_buf, 2);
	ts_info("0x52cc = 0x%02x%02x", tm_buf[0], tm_buf[1]);
	ret |= goodix_i2c_read(ts_dev, 0x52ce, tm_buf, 2);
	ts_info("0x52ce = 0x%02x02%x", tm_buf[0], tm_buf[1]);

	ret |= goodix_i2c_read(ts_dev, 0x52de, tm_buf, 8);
	offset = 0;
	for (i = 0; i < 8; i++) {
		offset += snprintf(&char_buf[offset], PAGE_SIZE,
			"%02x ", tm_buf[i]);
	}
	char_buf[offset] = 0;
	ts_info("0x52de = %s", &char_buf);

	for (i = 0; i < 5; i++) {
		ret |= goodix_i2c_read(ts_dev, 0x5436, tm_buf, 2);
		ts_info("0x5436 = 0x%02x%02x", tm_buf[0], tm_buf[1]);
	}

	ret |= goodix_i2c_read(ts_dev, 0x5639, tm_buf, 2);
	ts_info("0x5639 = 0x%02x%02x", tm_buf[0], tm_buf[1]);
	ret |= goodix_i2c_read(ts_dev, 0x563b, tm_buf, 1);
	ts_info("0x563b = 0x%02x", tm_buf[0]);

	for (i = 0; i < 5; i++) {
		ret |= goodix_i2c_read(ts_dev, 0x59d5, tm_buf, 2);
		ts_info("0x59d5 = 0x%02x%02x", tm_buf[0], tm_buf[1]);
	}

	ret |= goodix_i2c_read(ts_dev, 0x59dd, tm_buf, 2);
	ts_info("0x59dd = 0x%02x%02x", tm_buf[0], tm_buf[1]);
	ret |= goodix_i2c_read(ts_dev, 0x59df, tm_buf, 2);
	ts_info("0x59df = 0x%02x%02x", tm_buf[0], tm_buf[1]);
	ret |= goodix_i2c_read(ts_dev, 0x59e1, tm_buf, 1);
	ts_info("0x59e1 = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x59e2, tm_buf, 1);
	ts_info("0x59e2 = 0x%02x", tm_buf[0]);

	ret |= goodix_i2c_read(ts_dev, 0x5a6a, tm_buf, 12);
	offset = 0;
	for (i = 0; i < 12; i++) {
		offset += snprintf(&char_buf[offset], PAGE_SIZE,
			"%02x ", tm_buf[i]);
	}
	char_buf[offset] = 0;
	ts_info("0x5a6a = %s", &char_buf);

	ret |= goodix_i2c_read(ts_dev, 0x5b4e, tm_buf, 1);
	ts_info("0x5b4e = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x5bbc, tm_buf, 1);
	ts_info("0x5bbc = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x5c7c, tm_buf, 1);
	ts_info("0x5c7c = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x5c90, tm_buf, 1);
	ts_info("0x5c90 = 0x%02x", tm_buf[0]);

	ret |= goodix_i2c_read(ts_dev, 0x5e19, tm_buf, 9);
	offset = 0;
	for (i = 0; i < 9; i++) {
		offset += snprintf(&char_buf[offset], PAGE_SIZE,
			"%02x ", tm_buf[i]);
	}
	char_buf[offset] = 0;
	ts_info("0x5e19 = %s", &char_buf);

	ret |= goodix_i2c_read(ts_dev, 0x5ef0, tm_buf, 1);
	ts_info("0x5ef0 = 0x%02x", tm_buf[0]);
	ret |= goodix_i2c_read(ts_dev, 0x5f23, tm_buf, 2);
	ts_info("0x5f23 = 0x%02x%02x", tm_buf[0], tm_buf[1]);
	ret |= goodix_i2c_read(ts_dev, 0x5f42, tm_buf, 1);
	ts_info("0x5f42 = 0x%02x", tm_buf[0]);

	ret |= goodix_i2c_read(ts_dev, 0x6ea0, tm_buf, 12);
	offset = 0;
	for (i = 0; i < 12; i++) {
		offset += snprintf(&char_buf[offset], PAGE_SIZE,
			"%02x ", tm_buf[i]);
	}
	char_buf[offset] = 0;
	ts_info("0x6ea0 = %s", &char_buf);

	ret |= goodix_i2c_read(ts_dev, 0x6f6f, tm_buf, 1);
	ts_info("0x6f6f = 0x%02x", tm_buf[0]);

	for (i = 0; i < 2; i++) {
		ret |= goodix_i2c_read(ts_dev, 0xbed0, tm_buf, 1);
		ts_info("0xbed0 = 0x%02x", tm_buf[0]);
	}

	if (ret) {
		ts_err("Read reg fail.");
		return ret;
	}

	return ret;
}

