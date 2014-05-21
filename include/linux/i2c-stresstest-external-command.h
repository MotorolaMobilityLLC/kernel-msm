/*
        I2C Stress Test include file

*/
#ifndef __I2C_STRESSTEST_EXTERNAL_COMMAND_H__
#define __I2C_STRESSTEST_EXTERNAL_COMMAND_H__

typedef enum{
	I2C_SELFTEST_TEST_CASE_COMMAND,
	I2C_SELFTEST_TEST_SET_COMMAND,
	I2C_SELFTEST_TEST_RUN_COMMAND,
	I2C_SELFTEST_TEST_MODULE_COMMAND,
}AXE_I2C_STRESS_TEST_EXTERNAL_COMMAND;

extern int i2c_stresstest_print_status(const char *buf);
extern ssize_t i2c_stresstest_command_parser(const char *buf, size_t count);

#endif //__I2C_STRESSTEST_EXTERNAL_COMMAND_H__

