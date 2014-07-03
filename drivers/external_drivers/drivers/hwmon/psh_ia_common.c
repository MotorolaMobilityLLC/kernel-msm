/*
 *  psh_ia_common.c  - Intel PSH IA side driver common lib
 *
 * Copyright 2012-2013 Intel Corporation All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA
 */

#include <linux/ctype.h>
#include "psh_ia_common.h"
#include <asm/intel-mid.h>
#include <linux/hrtimer.h>

#define TOLOWER(x) ((x) | 0x20)
/* translate string to unsigned long value */
unsigned long _strtoul(const char *str, char **endp, unsigned int base)
{
	unsigned long value = 0;

	if (base == 0) {
		if (str[0] == '0') {
			if (TOLOWER(str[1]) == 'x' && isxdigit(str[2]))
				base = 16;
			else
				base = 8;
		} else
			base = 10;
	}

	if (base == 16 && str[0] == '0' && TOLOWER(str[1]) == 'x')
		str += 2;

	while (isxdigit(*str)) {
		unsigned int temp_value;

		if ('0' <= *str && *str <= '9')
			temp_value = *str - '0';
		else
			temp_value = TOLOWER(*str) - 'a' + 10;

		if (temp_value >= base)
			break;

		value = value * base + temp_value;
		str++;
	}

	if (endp)
		*endp = (char *)str;

	return value;
}

long trans_strtol(const char *str, char **endp, unsigned int base)
{
	if (*str == '-')
		return -_strtoul(str + 1, endp, base);

	return _strtoul(str, endp, base);
}

void ia_lbuf_read_init(struct loop_buffer *lbuf,
		u8 *buf, u16 size, update_finished_f uf)
{
	lbuf->addr = buf;
	lbuf->length = size;
	lbuf->off_head = lbuf->off_tail = 0;
	lbuf->update_finished = uf;
	lbuf->in_reading = 0;
}

void ia_lbuf_read_reset(struct loop_buffer *lbuf)
{
	if (lbuf) {
		lbuf->off_head = lbuf->off_tail = 0;
		lbuf->in_reading = 0;
	}
}

int ia_lbuf_read_next(struct psh_ia_priv *psh_ia_data,
			struct loop_buffer *lbuf,
			u8 **buf, u16 *size)
{
	struct frame_head *fhead =
			(struct frame_head *)(lbuf->addr + lbuf->off_head);

	*buf = NULL;
	*size = 0;

	if (lbuf->in_reading) {
		lbuf->in_reading = 0;

		/* go over previous frame has been read */
		lbuf->off_head += frame_size(fhead->length);
		lbuf->off_tail = lbuf->off_head;
		fhead = (struct frame_head *)(lbuf->addr + lbuf->off_head);
	}

	if (fhead->sign == LBUF_DISCARD_SIGN) {
		fhead = (struct frame_head *)lbuf->addr;
		lbuf->off_head = lbuf->off_tail = 0;
	}

	if (fhead->sign == LBUF_CELL_SIGN) {
		if (fhead->length > LBUF_MAX_CELL_SIZE)
			goto f_out;

		*buf = lbuf->addr + lbuf->off_head + sizeof(*fhead);
		*size = fhead->length;
		lbuf->in_reading = 1;
	}

f_out:
	if (lbuf->update_finished &&
			(!lbuf->in_reading && lbuf->off_head)) {
		/* no more data frame, inform FW to update its HEAD */
		lbuf->update_finished(psh_ia_data, lbuf->off_head);
	}

	return !lbuf->in_reading;
}

void ia_circ_reset_off(struct circ_buf *circ)
{
	circ->head = 0;
	circ->tail = 0;
}

void ia_circ_put_data(struct circ_buf *circ, const char *buf, u32 size)
{
	int tail_size, cnt;

	if (CIRC_SPACE(circ->head, circ->tail, CIRC_SIZE) < size)
		return;
	tail_size = CIRC_SPACE_TO_END(circ->head, circ->tail, CIRC_SIZE);
	cnt = size;
	if (cnt > tail_size)
		cnt = tail_size;
	memcpy(circ->buf + circ->head, buf, cnt);
	cnt = size - cnt;
	if (cnt)
		memcpy(circ->buf, buf + tail_size, cnt);
	circ->head += size;
	circ->head &= (CIRC_SIZE - 1);
}

void ia_circ_dbg_put_data(struct psh_ia_priv *psh_ia_data,
					 const char *buf, u32 size)
{
	int temp_count;
	struct circ_buf *circ;

	if (size > CIRC_SIZE -1)
		return;

	temp_count = 0;
	circ = &psh_ia_data->circ_dbg;

	if (CIRC_SPACE(circ->head, circ->tail, CIRC_SIZE) < size) {

		mutex_lock(&psh_ia_data->circ_dbg_mutex);

		circ->tail = circ->head + size + 1;
		circ->tail &= (CIRC_SIZE - 1);
		while (circ->buf[circ->tail++] != '\n') {
			circ->tail &= (CIRC_SIZE - 1);
			temp_count++;
			if (temp_count > (CIRC_SIZE -1))
				break;
		}

		mutex_unlock(&psh_ia_data->circ_dbg_mutex);
	}

	ia_circ_put_data(circ, buf, size);
}

int ia_circ_get_data(struct circ_buf *circ, char *buf, u32 size)
{
	int avail, avail_tail, cnt;

	avail = CIRC_CNT(circ->head, circ->tail, CIRC_SIZE);
	if (!avail)
		return 0;
	avail_tail = CIRC_CNT_TO_END(circ->head, circ->tail, CIRC_SIZE);
	if (avail_tail) {
		cnt = size;
		if (cnt > avail_tail)
			cnt = avail_tail;
		memcpy(buf, circ->buf + circ->tail, cnt);
		size -= cnt;
		avail -= cnt;
		circ->tail += cnt;
		if (!avail || !size)
			return cnt;
	}
	cnt = size;
	if (cnt > avail)
		cnt = avail;
	memcpy(buf + avail_tail, circ->buf, cnt);
	circ->tail += cnt;
	circ->tail &= (CIRC_SIZE - 1);
	return avail_tail + cnt;
}

int ia_circ_dbg_get_data(struct psh_ia_priv *psh_ia_data, char *buf, u32 size)
{
	int cnt;
	struct circ_buf *circ = &psh_ia_data->circ_dbg;

	mutex_lock(&psh_ia_data->circ_dbg_mutex);
	cnt = ia_circ_get_data(circ, buf, size);
	mutex_unlock(&psh_ia_data->circ_dbg_mutex);

	return cnt;
}

static int ia_sync_timestamp(struct psh_ia_priv *psh_ia_data, u8 check_interval)
{
	static u64 tick_old = 0;
	struct ia_cmd cmd_timestamp = { 0 };
	struct cmd_ia_notify_param *param = (struct cmd_ia_notify_param *)cmd_timestamp.param;
	u8 *linux_base_ns = param->extra;
	timestamp_t base_ns = 0;
	int ret;

	if (check_interval) {
		if (!tick_old) {
			tick_old = jiffies;
			return 0;
		} else {
			if (jiffies - tick_old < (120 * HZ))
				return 0;
		}
	}

	cmd_timestamp.tran_id = 0;
	cmd_timestamp.cmd_id = CMD_IA_NOTIFY;
	cmd_timestamp.sensor_id = 0;
	param->id = IA_NOTIFY_TIMESTAMP_SYNC;
	base_ns = ktime_to_ns(ktime_get_boottime());
	tick_old = jiffies;
	*(timestamp_t *)linux_base_ns = base_ns;
	ret = process_send_cmd(psh_ia_data, PSH2IA_CHANNEL0,
			&cmd_timestamp, sizeof(struct ia_cmd) - CMD_PARAM_MAX_SIZE
		+ sizeof(struct cmd_ia_notify_param) + sizeof(base_ns));
	return ret;
}

int ia_send_cmd(struct psh_ia_priv *psh_ia_data,
			struct ia_cmd *cmd, int len)
{
	int ret = 0;
	static struct resp_cmd_ack cmd_ack;

	ia_sync_timestamp(psh_ia_data, 0);

	mutex_lock(&psh_ia_data->cmd_mutex);
	if (cmd->cmd_id == CMD_RESET) {
		cmd->tran_id = 0;
		ia_circ_reset_off(&psh_ia_data->circ);
	}

	cmd_ack.cmd_id = cmd->cmd_id;
	psh_ia_data->cmd_ack = &cmd_ack;

	psh_ia_data->cmd_in_progress = cmd->cmd_id;
	ret = process_send_cmd(psh_ia_data, PSH2IA_CHANNEL0,
			cmd, len);
	psh_ia_data->cmd_in_progress = CMD_INVALID;
	if (ret) {
		psh_err("send cmd (id = %d) failed, ret=%d\n",
				cmd->cmd_id, ret);
		goto f_out;
	}
	if (cmd->cmd_id == CMD_FW_UPDATE)
		goto f_out;

ack_wait:
	if (!wait_for_completion_timeout(&psh_ia_data->cmd_comp,
				5 * HZ)) {
		psh_err("no CMD_ACK for %d back, timeout!\n",
				cmd_ack.cmd_id);
		ret = -ETIMEDOUT;
	} else if (cmd_ack.ret) {
		if (cmd_ack.ret == E_CMD_ASYNC)
			goto ack_wait;
		psh_err("CMD %d return error %d!\n", cmd_ack.cmd_id,
				cmd_ack.ret);
		ret = -EREMOTEIO;
	}

f_out:
	psh_ia_data->cmd_ack = NULL;
	mutex_unlock(&psh_ia_data->cmd_mutex);
	return ret;
}

ssize_t ia_start_control(struct device *dev,
			struct device_attribute *attr,
			const char *str, size_t count)
{
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);
	struct ia_cmd cmd_user = { 0 };
	u8 *ptr = (u8 *)&cmd_user;
	char *s;
	long val;
	int token = 0;
	int ret;

	while (*str && (token < sizeof(cmd_user))) {
		val = trans_strtol(str, &s, 0);
		if (str == s) {
			str++;
			continue;
		}
		str = s;
		*ptr++ = (u8)val;
		token++;
	}

	if (cmd_user.cmd_id == CMD_SETUP_DDR) {
		ret = do_setup_ddr(dev);
		if (ret) {
			psh_err("do_setup_ddr failed\n");
			return ret;
		} else {
			return count;
		}
	}

	ret = ia_send_cmd(psh_ia_data, &cmd_user, token);
	if (ret)
		return ret;
	else
		return count;
}

ssize_t ia_read_data_size(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);
	int avail = CIRC_CNT(psh_ia_data->circ.head,
				psh_ia_data->circ.tail, CIRC_SIZE);

	return snprintf(buf, PAGE_SIZE, "%d\n", avail);
}

ssize_t ia_read_data(struct file *file, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);

	return ia_circ_get_data(&psh_ia_data->circ, buf, count);
}

ssize_t ia_read_debug_data(struct file *file, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);

	return ia_circ_dbg_get_data(psh_ia_data, buf, count);
}

ssize_t ia_set_dbg_mask(struct device *dev,
			struct device_attribute *attr,
			const char *str, size_t count)
{
	struct ia_cmd cmd;
	struct cmd_debug_param *param = (struct cmd_debug_param *)cmd.param;
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);
	int token = 0;
	char *s;
	long val;

	while (*str) {
		val = trans_strtol(str, &s, 0);
		if (str == s) {
			str++;
			continue;
		}
		switch (token) {
		case 0:
			param->mask_out = val;
			break;
		case 1:
			param->mask_level = val;
			break;
		default:
			break;
		}
		str = s;
		if (++token == 2)
			break;
	}

	if (token == 2) {
		int ret;
		cmd.cmd_id = CMD_DEBUG;
		cmd.sensor_id = 0;
		param->sub_cmd = SCMD_DEBUG_SET_MASK;
		ret = ia_send_cmd(psh_ia_data, &cmd, 10);
		if (ret)
			return ret;
		else
			return count;
	} else {
		psh_err("wrong input for \"<mask_out> <mask_level>\"\n");
		return -EPERM;
	}
}

ssize_t ia_get_dbg_mask(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ia_cmd cmd;
	struct cmd_debug_param *param = (struct cmd_debug_param *)cmd.param;
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);
	int ret;

	cmd.cmd_id = CMD_DEBUG;
	cmd.sensor_id = 0;
	param->sub_cmd = SCMD_DEBUG_GET_MASK;
	ret = ia_send_cmd(psh_ia_data, &cmd, 8);
	if (ret)
		return snprintf(buf, PAGE_SIZE, "%s failed!\n", __func__);

	return snprintf(buf, PAGE_SIZE, "mask_out:0x%x mask_level:0x%x\n",
			psh_ia_data->dbg_mask.mask_out,
			psh_ia_data->dbg_mask.mask_level);
}

static inline int is_port_sensor(enum sensor_type type)
{
	return ((type > PORT_SENSOR_BASE) && (type < PORT_SENSOR_MAX_NUM));
}

static void ia_handle_snr_info(struct psh_ia_priv *psh_ia_data,
				const struct snr_info *sinfo)
{
	char buf[STR_BUFF_SIZE];
	ssize_t str_size;
	int i;
	static int snr_info_start;
	static int sensor_map_setup;

	if (!snr_info_start) {
		snr_info_start++;
		str_size = snprintf(buf, STR_BUFF_SIZE,
				"******** Start Sensor Status ********\n");
		ia_circ_dbg_put_data(psh_ia_data, buf, str_size);
	}

	if (!sinfo) {
		if (snr_info_start) {
			snr_info_start = 0;
			sensor_map_setup = 1;
			str_size = snprintf(buf, STR_BUFF_SIZE,
					"******** End Sensor Status ********\n");
			ia_circ_dbg_put_data(psh_ia_data, buf, str_size);
		}
		return;
	}

	if (!sensor_map_setup && snr_info_start) {
		int len = strlen(sinfo->name) + 1;
		struct sensor_db *sensor_obj =
				kmalloc(sizeof(struct sensor_db), GFP_KERNEL);

		if (sensor_obj == NULL) {
			psh_err("ia_handle_snr_info failed kmalloc sensor_obj\n");
			BUG();
			return;
		}

		sensor_obj->sid = sinfo->id;
		memcpy(sensor_obj->sensor_name, sinfo->name,
			len < SNR_NAME_MAX_LEN ? len : SNR_NAME_MAX_LEN);
		sensor_obj->sensor_name[SNR_NAME_MAX_LEN - 1] = '\0';
		list_add_tail(&sensor_obj->list, &psh_ia_data->sensor_list);

	}

	str_size = snprintf(buf, STR_BUFF_SIZE,
			"***** Sensor %5s(%d) Status *****\n",
			sinfo->name, sinfo->id);
	ia_circ_dbg_put_data(psh_ia_data, buf, str_size);

	str_size = snprintf(buf, STR_BUFF_SIZE,
			"  freq=%d, freq_max=%d\n"
			"  status=0x%x,  bit_cfg=0x%x\n"
			"  data_cnt=%d,  priv=0x%x\n"
			"  attri=0x%x, health=%d\n",
			sinfo->freq, sinfo->freq_max,
			sinfo->status, sinfo->bit_cfg,
			sinfo->data_cnt, sinfo->priv,
			sinfo->attri, sinfo->health);
	ia_circ_dbg_put_data(psh_ia_data, buf, str_size);

	for (i = 0; i < sinfo->link_num; i++) {
		const struct link_info *linfo = &sinfo->linfo[i];
		str_size = snprintf(buf, STR_BUFF_SIZE,
			"    %s%s=%3d, rpt_freq=%d\n",
			(linfo->ltype == LINK_AS_REPORTER) ?
						"REPORTER" : "CLIENT",
			(linfo->ltype == LINK_AS_MONITOR) ?
						"(M)" : "",
			linfo->sid,
			linfo->rpt_freq);

		ia_circ_dbg_put_data(psh_ia_data, buf, str_size);
	}

	str_size = snprintf(buf, STR_BUFF_SIZE,
			"*****************************\n");
	ia_circ_dbg_put_data(psh_ia_data, buf, str_size);
}

ssize_t ia_set_status_mask(struct device *dev,
			struct device_attribute *attr,
			const char *str, size_t count)
{
	char *s;
	long val = 0;
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);

	while (*str) {
		val = trans_strtol(str, &s, 0);
		if (str == s) {
			str++;
			continue;
		} else
			break;
	}
	psh_ia_data->status_bitmask = val;

	pr_debug("set status_bitmask as 0x%x\n", psh_ia_data->status_bitmask);
	return count;
}

ssize_t ia_get_status_mask(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "status_mask=0x%x\n",
						psh_ia_data->status_bitmask);
}

ssize_t ia_trig_get_status(struct device *dev,
			struct device_attribute *attr,
			const char *str, size_t count)
{
	struct ia_cmd cmd;
	struct get_status_param *param = (struct get_status_param *)cmd.param;
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);
	int ret;

	if (str[0] == 'm')
		param->snr_bitmask = psh_ia_data->status_bitmask;
	else if (str[0] == 'a')
		param->snr_bitmask = (u32)-1;
	else if (str[0] == 'r')
		param->snr_bitmask = ((u32)-1) & ~SNR_RUNONLY_BITMASK;

	cmd.cmd_id = CMD_GET_STATUS;
	cmd.sensor_id = 0;
	ret = ia_send_cmd(psh_ia_data, &cmd, 7);
	if (ret)
		return ret;

	return count;
}

ssize_t ia_get_counter(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	struct ia_cmd cmd;
	struct cmd_counter_param *param = (struct cmd_counter_param *)cmd.param;
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);

	cmd.cmd_id = CMD_COUNTER;
	cmd.sensor_id = 0;
	param->sub_cmd = SCMD_GET_COUNTER;

	ret = ia_send_cmd(psh_ia_data, &cmd, 5);
	if (ret)
		return ret;

	return snprintf(buf, PAGE_SIZE, "   Module        Counter(hex)\n"
					"    GPIO          %x\n"
					"    I2C           %x\n"
					"    DMA           %x\n"
					"    PRINT         %x\n",
			psh_ia_data->counter.gpio_counter,
			psh_ia_data->counter.i2c_counter,
			psh_ia_data->counter.dma_counter,
			psh_ia_data->counter.print_counter);
}
ssize_t ia_clear_counter(struct device *dev,
			struct device_attribute *attr,
			const char *str, size_t count)
{
	int ret;
	struct ia_cmd cmd;
	struct cmd_counter_param *param = (struct cmd_counter_param *)cmd.param;
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);

	cmd.cmd_id = CMD_COUNTER;
	cmd.sensor_id = 0;
	param->sub_cmd = SCMD_CLEAR_COUNTER;

	ret = ia_send_cmd(psh_ia_data, &cmd, 5);
	if (ret)
		return ret;
	else
		return count;
}

ssize_t ia_get_version(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	struct ia_cmd cmd;
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);
	cmd.cmd_id = CMD_GET_VERSION;
	cmd.sensor_id = 0;

	ret = ia_send_cmd(psh_ia_data, &cmd, 3);
	if (ret)
		return ret;

	return snprintf(buf, PAGE_SIZE, "%s\n", psh_ia_data->version_str);
}
static SENSOR_DEVICE_ATTR(status_mask, S_IRUSR | S_IWUSR,
				ia_get_status_mask, ia_set_status_mask, 0);
static SENSOR_DEVICE_ATTR(status_trig, S_IWUSR, NULL, ia_trig_get_status, 1);
static SENSOR_DEVICE_ATTR(debug, S_IRUSR | S_IWUSR ,
				ia_get_dbg_mask, ia_set_dbg_mask, 0);
static SENSOR_DEVICE_ATTR(control, S_IWUSR, NULL, ia_start_control, 1);
static SENSOR_DEVICE_ATTR(data_size, S_IRUSR, ia_read_data_size, NULL, 2);
static SENSOR_DEVICE_ATTR(counter, S_IWUSR | S_IRUSR, ia_get_counter,
				ia_clear_counter, 0);
static SENSOR_DEVICE_ATTR(fw_version, S_IRUSR, ia_get_version, NULL, 0);

static struct bin_attribute bin_attr = {
	.attr = { .name = "data", .mode = S_IRUSR },
	.read = ia_read_data
};
static struct bin_attribute dbg_attr = {
	.attr = { .name = "trace", .mode = S_IRUSR },
	.read = ia_read_debug_data
};

const char *sensor_get_name(u8 sid, struct psh_ia_priv *psh_ia_data)
{
	if (sid == PSH_ITSELF)
		return "_PSH_";
	else if (is_port_sensor(sid))
		return sensor_port_str[sid - PORT_SENSOR_BASE - 1];
	else {
		struct sensor_db *sensor_obj;

		list_for_each_entry(sensor_obj,
				&psh_ia_data->sensor_list, list) {
			if (sid == sensor_obj->sid)
				return sensor_obj->sensor_name;
		}
	}

	return "?????";
}

static const char level_str[8][6] = {
	"FATAL",
	"ERROR",
	" WARN",
	" INFO",
	"DEBUG",
	"C_TRC",
	"D_TRC",
	"M_TRC",
};

static const char ctrace_str[12][6] = {
	"SCORE",
	"S_CFG",
	"HW_PD",
	"HW_PU",
	"HW_PI",
	" C_IN",
	"C_OUT",
	"IIDLE",
	"OIDLE",
	"SUSPD",
	"RESUM",
};

static const char dtrace_str[5][6] = {
	" MISC",
	"D_RDY",
	" D_IN",
	"D_OUT",
	"DDUMP"
};

static const char mtrace_str[3][6] = {
	"TLOCK",
	" LOCK",
	"ULOCK",
};

const char *_get_evt_str(u16 level, u16 evt)
{
	if (level == PSH_DBG_CTRACE)
		return ctrace_str[evt];
	else if (level == PSH_DBG_DTRACE)
		return dtrace_str[evt];
	if (level == PSH_DBG_MTRACE)
		return mtrace_str[evt];
	else {
		int level_index = 0;
		u16 mid_val = level;

		while (mid_val > 1) {
			mid_val /= 2;
			level_index++;
		}
		return level_str[level_index];
	}
}

/*
 * return value = 0	no valid data frame
 * return value > 0	data frame
 * return value < 0	error data frame
 */
int ia_handle_frame(struct psh_ia_priv *psh_ia_data, void *dbuf, int size)
{
	struct cmd_resp *resp = dbuf;
	const struct snr_info *sinfo;
	const struct resp_version *version;
	const struct resp_cmd_ack *cmd_ack;
	u32 curtime;
	int len;
	const char *sensor_name;
	const char *event_name;
	const char *context_name;
	struct trace_data *out_data;
	char msg_str[STR_BUFF_SIZE];

	ia_sync_timestamp(psh_ia_data, 1);

	switch (resp->type) {
	case RESP_CMD_ACK:
		cmd_ack = (struct resp_cmd_ack *)resp->buf;
		if (!psh_ia_data->cmd_ack)
			psh_err("Unexpected CMD_ACK recevied, %d\n", cmd_ack->cmd_id);
		else if (cmd_ack->cmd_id == psh_ia_data->cmd_ack->cmd_id) {
			psh_ia_data->cmd_ack->ret = cmd_ack->ret;
			complete(&psh_ia_data->cmd_comp);
		} else
			psh_err("Unmatched CMD_ACK recevied, %d(EXP: %d)\n",
					cmd_ack->cmd_id,
					psh_ia_data->cmd_ack->cmd_id);
		return 0;
	case RESP_BIST_RESULT:
		break;
	case RESP_DEBUG_MSG:
		ia_circ_dbg_put_data(psh_ia_data,
				resp->buf, resp->data_len);
		return 0;
	case RESP_GET_STATUS:
		sinfo = (struct snr_info *)resp->buf;
		if (!resp->data_len)
			ia_handle_snr_info(psh_ia_data, NULL);
		else if (SNR_INFO_SIZE(sinfo) == resp->data_len)
			ia_handle_snr_info(psh_ia_data, sinfo);
		else {
			psh_err("Wrong RESP_GET_STATUS!\n");
			return 0;
		}
		break;
	case RESP_DEBUG_GET_MASK:
		memcpy(&psh_ia_data->dbg_mask, resp->buf,
				sizeof(psh_ia_data->dbg_mask));
		return 0;
	case RESP_COUNTER:
		memcpy(&psh_ia_data->counter, resp->buf,
				sizeof(psh_ia_data->counter));
		return 0;
	case RESP_GET_VERSION:
		version = (struct resp_version *)resp->buf;
		if (likely(version->str_len < VERSION_STR_MAX_SIZE))
			memcpy(psh_ia_data->version_str, version->str,
					version->str_len + 1);
		else {
			memcpy(psh_ia_data->version_str, version->str,
					VERSION_STR_MAX_SIZE - 1);
			psh_ia_data->version_str[VERSION_STR_MAX_SIZE - 1]
				= '\0';
		}
		return 0;
	case RESP_TRACE_MSG:
		out_data = (struct trace_data *)resp->buf;
		while ((char *)out_data < resp->buf + resp->data_len) {
			curtime = out_data->timestamp;
			sensor_name = sensor_get_name(out_data->sensor_id,
								psh_ia_data);
			event_name = _get_evt_str(out_data->type,
					out_data->event);
			context_name = sensor_get_name(out_data->sensor_cnt,
								psh_ia_data);

			len = snprintf(msg_str, STR_BUFF_SIZE,
						"[%u,%s,%s,%s]\n",
						curtime, sensor_name,
						event_name, context_name);

			ia_circ_dbg_put_data(psh_ia_data,
					msg_str, len);
			out_data++;
		}
		return 0;
	default:
		break;
	}

	pr_debug("one DDR frame, data of sensor %d, size %d\n",
			resp->sensor_id, size);
	ia_circ_put_data(&psh_ia_data->circ, dbuf, size);
	return size;
}

int psh_ia_common_init(struct device *dev, struct psh_ia_priv **data)
{
	int ret = -1;
	struct psh_ia_priv *psh_ia_data;

	psh_ia_data = kzalloc(sizeof(*psh_ia_data), GFP_KERNEL);
	if (!psh_ia_data) {
		dev_err(dev, "can not allocate psh_ia_data\n");
		goto priv_err;
	}
	*data = psh_ia_data;

	mutex_init(&psh_ia_data->cmd_mutex);
	psh_ia_data->cmd_in_progress = CMD_INVALID;
	mutex_init(&psh_ia_data->circ_dbg_mutex);
	init_completion(&psh_ia_data->cmd_load_comp);
	init_completion(&psh_ia_data->cmd_comp);
	INIT_LIST_HEAD(&psh_ia_data->sensor_list);

	psh_ia_data->circ.buf = kmalloc(CIRC_SIZE, GFP_KERNEL);
	if (!psh_ia_data->circ.buf) {
		dev_err(dev, "can not allocate circ buffer\n");
		goto circ_err;
	}

	psh_ia_data->circ_dbg.buf = kmalloc(CIRC_SIZE, GFP_KERNEL);
	if (!psh_ia_data->circ_dbg.buf) {
		dev_err(dev, "can not allocate circ buffer\n");
		goto dbg_err;
	}

	psh_ia_data->version_str = kmalloc(VERSION_STR_MAX_SIZE, GFP_KERNEL);
	if (!psh_ia_data->version_str) {
		dev_err(dev, "can not allocate version string\n");
		goto ver_err;
	}

	psh_ia_data->status_bitmask = ((u32)-1) & ~SNR_RUNONLY_BITMASK;

	dev_set_drvdata(dev, psh_ia_data);

	ret = sysfs_create_file(&dev->kobj,
			&sensor_dev_attr_status_mask.dev_attr.attr);
	ret += sysfs_create_file(&dev->kobj,
			&sensor_dev_attr_status_trig.dev_attr.attr);
	ret += sysfs_create_file(&dev->kobj,
			&sensor_dev_attr_debug.dev_attr.attr);
	ret += sysfs_create_file(&dev->kobj,
			&sensor_dev_attr_control.dev_attr.attr);
	ret += sysfs_create_file(&dev->kobj,
			&sensor_dev_attr_data_size.dev_attr.attr);
	ret += sysfs_create_file(&dev->kobj,
			&sensor_dev_attr_counter.dev_attr.attr);
	ret += sysfs_create_file(&dev->kobj,
			&sensor_dev_attr_fw_version.dev_attr.attr);
	ret += sysfs_create_bin_file(&dev->kobj, &bin_attr);
	ret += sysfs_create_bin_file(&dev->kobj, &dbg_attr);
	if (ret) {
		dev_err(dev, "can not create sysfs\n");
		goto sysfs_err;
	}

	return 0;

sysfs_err:
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_status_mask.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_status_trig.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_debug.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_control.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_data_size.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_counter.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_fw_version.dev_attr.attr);
	sysfs_remove_bin_file(&dev->kobj, &bin_attr);
	sysfs_remove_bin_file(&dev->kobj, &dbg_attr);

	kfree(psh_ia_data->version_str);
ver_err:
	kfree(psh_ia_data->circ_dbg.buf);
dbg_err:
	kfree(psh_ia_data->circ.buf);
circ_err:
	kfree(psh_ia_data);
priv_err:
	return ret;
}

void psh_ia_common_deinit(struct device *dev)
{
	struct sensor_db *sensor_obj, *sensor_tmp;
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);

	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_status_mask.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_status_trig.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_debug.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_control.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_data_size.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_counter.dev_attr.attr);
	sysfs_remove_file(&dev->kobj,
		&sensor_dev_attr_fw_version.dev_attr.attr);
	sysfs_remove_bin_file(&dev->kobj, &bin_attr);
	sysfs_remove_bin_file(&dev->kobj, &dbg_attr);

	list_for_each_entry_safe(sensor_obj, sensor_tmp,
				&psh_ia_data->sensor_list, list) {
		list_del(&sensor_obj->list);
		kfree(sensor_obj);
	}
	kfree(psh_ia_data->version_str);

	kfree(psh_ia_data->circ.buf);

	kfree(psh_ia_data->circ_dbg.buf);

	kfree(psh_ia_data);
}

int psh_ia_comm_suspend(struct device *dev)
{
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);
	struct ia_cmd cmd = {
		.cmd_id = CMD_IA_NOTIFY,
	};
	struct cmd_ia_notify_param *param =
			(struct cmd_ia_notify_param *)cmd.param;
	int ret;

	param->id = IA_NOTIFY_SUSPEND;
	ret = ia_send_cmd(psh_ia_data, &cmd, 4);
	if (ret)
		dev_warn(dev, "PSH: IA_NOTIFY_SUSPEND ret=%d\n", ret);
	return 0;
}

int psh_ia_comm_resume(struct device *dev)
{
	struct psh_ia_priv *psh_ia_data =
			(struct psh_ia_priv *)dev_get_drvdata(dev);
	struct ia_cmd cmd = {
		.cmd_id = CMD_IA_NOTIFY,
	};
	struct cmd_ia_notify_param *param =
			(struct cmd_ia_notify_param *)cmd.param;
	int ret;

	param->id = IA_NOTIFY_RESUME;
	ret = ia_send_cmd(psh_ia_data, &cmd, 4);
	if (ret)
		dev_warn(dev, "PSH: IA_NOTIFY_RESUME ret=%d\n", ret);
	return 0;
}
