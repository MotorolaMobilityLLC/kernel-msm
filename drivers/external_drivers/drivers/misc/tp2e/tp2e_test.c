#define TP2E_CRASH_DATA_LEN 10

static char trace_tp2e_crash_str[4 * (TP2E_CRASH_DATA_LEN + 1)] = {'\0',};

static int set_tp2e_crash(const char *val, struct kernel_param *kp)
{
	char ev_name[TP2E_CRASH_DATA_LEN],
	  data0[TP2E_CRASH_DATA_LEN],
	  data1[TP2E_CRASH_DATA_LEN],
	  data2[TP2E_CRASH_DATA_LEN];
	int ret = -EINVAL;

	if (sscanf(val, "%s %s %s %s", ev_name, data0, data1, data2) != 4)
		return ret;

	memcpy(trace_tp2e_crash_str, val, strlen(val));

	trace_tp2e_generic_event(TP2E_EV_INFO, "tp2e_test", ev_name,
				 data0, data1, data2, "", "", "", "");

	return 0;
}

static int get_tp2e_crash(char *buf, struct kernel_param *kp)
{
	size_t len = strlen(trace_tp2e_crash_str);
	memcpy(buf, trace_tp2e_crash_str, len);
	return len;
}

module_param_call(trace_tp2e_crash_str, set_tp2e_crash, get_tp2e_crash, NULL, 0644);
MODULE_PARM_DESC(trace_tp2e_crash_str, "log trace tp2e crash <ev_name> <data0> <data1> <data2>");
