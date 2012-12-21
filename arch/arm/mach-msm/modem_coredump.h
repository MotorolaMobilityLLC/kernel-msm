#ifndef _MODEM_COREDUMP_HEADER
#define _MODEM_COREDUMP_HEADER

void *create_modem_coredump_device(const char *dev_name);
int do_modem_coredump(void *handle);

#endif
