/*
        I2C Stress Test include file

*/
#ifndef __AXI_I2C_TEST_CASE_H__
#define __AXI_I2C_TEST_CASE_H__
#include <linux/i2c.h>

#define I2C_TEST_PASS (0)
//if test fail, the return value should be negative.

//typedef int (*run_i2c_test)(struct i2c_client *apClient);

#define I2C_TEST_CASE_NAME_SIZE 20

#define ARRAY_AND_SIZE(x)       (x), ARRAY_SIZE(x)

#define __I2C_STRESS_TEST_CASE_ATTR(_name) { \
	.name = __stringify(_name),	\
	.run	= _name,					\
}

/**
 * struct i2c_test_case_info - for i2c stress test
 * @type: name for test case
 * @run: the main function for test
 *
 * The concrete of I2C test case struct should be added to i2c test run before 
 * running tests. Recommend to be called in probe function in i2c driver. The
 * function run should be implemented by i2c driver and the negative value(error
 * code) should be returned if the test fails. Otherwise, zero should be returned.
 */
struct i2c_test_case_info {
	const char		*name;
	int (*run)(struct i2c_client *apClient);
};


extern int i2c_add_test_case(struct i2c_client *apClient,
	  const char		*apTestSetName,
	  struct i2c_test_case_info const *apTestCaseList,
	  unsigned anNum);  

/*log function for logging in running tests*/
extern int i2c_log_in_test_case(
	const char *fmt, ...);
#endif //__AXI_I2C_TEST_CASE_H__