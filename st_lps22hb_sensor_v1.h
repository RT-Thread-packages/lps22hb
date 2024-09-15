

#ifndef SENSOR_ST_LPS22HB_H__
#define SENSOR_ST_LPS22HB_H__
#include <rtthread.h>
#include <rtdevice.h>

#if defined(RT_VERSION_CHECK)
    #if (RTTHREAD_VERSION >= RT_VERSION_CHECK(5, 0, 2))
        #define RT_SIZE_TYPE   rt_ssize_t
    #else
        #define RT_SIZE_TYPE   rt_size_t
    #endif
#endif
#include "lps22hb.h"

#define LPS22HB_ADDR_DEFAULT (0xBA >> 1)

int rt_hw_lps22hb_init(const char *name, struct rt_sensor_config *cfg);

#endif
