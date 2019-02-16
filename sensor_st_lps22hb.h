

#ifndef SENSOR_ST_LPS22HB_H__
#define SENSOR_ST_LPS22HB_H__

#include "sensor.h"
#include "lps22hb.h"

#define LPS22HB_ADDR_DEFAULT (0xBA >> 1)

int rt_hw_lps22hb_init(const char *name, struct rt_sensor_config *cfg);

#endif
