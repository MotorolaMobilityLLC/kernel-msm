/* HEADER */

#include <linux/types.h>

/* This needs to be thought through when we
add other sensors */

struct m4sensorhub_pressure_data {
	int pressure;
	int altitude;
	long long timestamp;
};
