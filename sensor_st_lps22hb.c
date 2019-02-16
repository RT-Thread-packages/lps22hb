
#include "sensor_st_lps22hb.h"

#define DBG_ENABLE
#define DBG_LEVEL DBG_LOG
#define DBG_SECTION_NAME  "sensor.st.lps22hb"
#define DBG_COLOR
#include <rtdbg.h>

static LPS22HB_Object_t lps22hb;
static struct rt_i2c_bus_device *i2c_bus_dev;
    
static int32_t i2c_init(void)
{
    return 0;
}

static int32_t lps22hb_get_tick(void)
{
    return rt_tick_get();
}

static int rt_i2c_write_reg(uint16_t addr, uint16_t reg, uint8_t *data, uint16_t len)
{
    rt_uint8_t tmp = reg;
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = addr;             /* Slave address */
    msgs[0].flags = RT_I2C_WR;        /* Write flag */
    msgs[0].buf   = &tmp;             /* Slave register address */
    msgs[0].len   = 1;                /* Number of bytes sent */

    msgs[1].addr  = addr;             /* Slave address */
    msgs[1].flags = RT_I2C_WR | RT_I2C_NO_START;        /* Read flag */
    msgs[1].buf   = data;             /* Read data pointer */
    msgs[1].len   = len;              /* Number of bytes read */

    if (rt_i2c_transfer(i2c_bus_dev, msgs, 2) != 2)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

static int rt_i2c_read_reg(uint16_t addr, uint16_t reg, uint8_t *data, uint16_t len)
{
    rt_uint8_t tmp = reg;
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = addr;             /* Slave address */
    msgs[0].flags = RT_I2C_WR;        /* Write flag */
    msgs[0].buf   = &tmp;             /* Slave register address */
    msgs[0].len   = 1;                /* Number of bytes sent */

    msgs[1].addr  = addr;             /* Slave address */
    msgs[1].flags = RT_I2C_RD;        /* Read flag */
    msgs[1].buf   = data;             /* Read data pointer */
    msgs[1].len   = len;              /* Number of bytes read */

    if (rt_i2c_transfer(i2c_bus_dev, msgs, 2) != 2)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t _lps22hb_set_odr(rt_sensor_t sensor, rt_uint16_t odr)
{
    if (sensor->info.type == RT_SENSOR_CLASS_BARO)
    {
        LPS22HB_PRESS_SetOutputDataRate(&lps22hb, odr);
    }
    else if (sensor->info.type == RT_SENSOR_CLASS_TEMP)
    {
        LPS22HB_TEMP_SetOutputDataRate(&lps22hb, odr);
    }

    return RT_EOK;
}

static rt_err_t _lps22hb_set_mode(rt_sensor_t sensor, rt_uint8_t mode)
{
    if (mode == RT_SENSOR_MODE_POLLING)
    {
        LPS22HB_FIFO_Set_Mode(&lps22hb, LPS22HB_FIFO_BYPASS_MODE);
//        LPS22HB_FIFO_Usage(&lps22hb, 0);
    }
    else if (mode == RT_SENSOR_MODE_INT)
    {
        LPS22HB_FIFO_Set_Mode(&lps22hb, LPS22HB_FIFO_BYPASS_MODE);
        lps22hb_drdy_on_int_set(&lps22hb.Ctx, PROPERTY_ENABLE);
    }
    else if (mode == RT_SENSOR_MODE_FIFO)
    {
        LPS22HB_FIFO_Set_Mode(&lps22hb, LPS22HB_FIFO_MODE);
        LPS22HB_FIFO_Set_Watermark_Level(&lps22hb, 32);
        LPS22HB_FIFO_Usage(&lps22hb, 1);
        LPS22HB_FIFO_Set_Interrupt(&lps22hb, 2);
    }
    else
    {
        LOG_W("Unsupported mode, code is %d", mode);
        return -RT_ERROR;
    }
    return RT_EOK;
}

static rt_err_t _lps22hb_set_power(rt_sensor_t sensor, rt_uint8_t power)
{
    if (power == RT_SENSOR_POWER_DOWN)
    {
        if (sensor->info.type == RT_SENSOR_CLASS_BARO)
        {
            LPS22HB_PRESS_Disable(&lps22hb);
        }
        else if (sensor->info.type == RT_SENSOR_CLASS_TEMP)
        {
            LPS22HB_TEMP_Disable(&lps22hb);
        }
    }
    else if (power == RT_SENSOR_POWER_NORMAL)
    {
        if (sensor->info.type == RT_SENSOR_CLASS_BARO)
        {
            LPS22HB_PRESS_Enable(&lps22hb);
        }
        else if (sensor->info.type == RT_SENSOR_CLASS_TEMP)
        {
            LPS22HB_TEMP_Enable(&lps22hb);
        }
    }
    else
    {
        LOG_W("Unsupported mode, code is %d", power);
        return -RT_ERROR;
    }
    return RT_EOK;
}

static rt_err_t _lps22hb_clear_int(struct rt_sensor_device *sensor)
{
    if (sensor->config.mode == RT_SENSOR_MODE_FIFO)
    {
        _lps22hb_set_mode(sensor, RT_SENSOR_MODE_POLLING);
        _lps22hb_set_mode(sensor, RT_SENSOR_MODE_FIFO);
    }
    LOG_D("clear int");
    return 0;
}

static rt_size_t _lps22hb_polling_get_data(rt_sensor_t sensor, struct rt_sensor_data *data)
{
    if (sensor->info.type == RT_SENSOR_CLASS_BARO)
    {
        float baro;
        LPS22HB_PRESS_GetPressure(&lps22hb, &baro);

        data->type = RT_SENSOR_CLASS_BARO;
        data->data.baro = baro * 100;
        data->timestamp = rt_sensor_get_ts();
    }
    else if (sensor->info.type == RT_SENSOR_CLASS_TEMP)
    {
        float temp_value;

        LPS22HB_TEMP_GetTemperature(&lps22hb, &temp_value);

        data->type = RT_SENSOR_CLASS_TEMP;
        data->data.temp = temp_value * 10;
        data->timestamp = rt_sensor_get_ts();
    }

    return 1;
}

static rt_size_t _lps22hb_fifo_get_data(rt_sensor_t sensor, struct rt_sensor_data *data, rt_size_t len)
{
    float baro,temp;
    rt_uint8_t i;
    struct rt_sensor_data *other_data;
    rt_sensor_t other_sensor;
    
    if (sensor == sensor->module->sen[0])
        other_sensor = sensor->module->sen[1];
    else
        other_sensor = sensor->module->sen[0];
        
    other_data = other_sensor->data_buf;
    
    if (len > 32) len = 32;
    
    for(i = 0; i < len; i++)
    {
        if (LPS22HB_FIFO_Get_Data(&lps22hb, &baro, &temp) == 0)
        {
            if (sensor->info.type == RT_SENSOR_CLASS_BARO)
            {
                data[i].type = RT_SENSOR_CLASS_BARO;
                data[i].data.baro = baro * 100;
                data[i].timestamp = rt_sensor_get_ts();
                
                other_data[i].type = RT_SENSOR_CLASS_TEMP;
                other_data[i].data.temp = temp * 10;
                other_data[i].timestamp = rt_sensor_get_ts();
            }
            else
            {
                data[i].type = RT_SENSOR_CLASS_TEMP;
                data[i].data.temp = temp * 10;
                data[i].timestamp = rt_sensor_get_ts();
                
                other_data[i].type = RT_SENSOR_CLASS_BARO;
                other_data[i].data.baro = baro * 100;
                other_data[i].timestamp = rt_sensor_get_ts();
            }
        }
        else
            break;
    }
    other_sensor->data_len = i * sizeof(struct rt_sensor_data);
    _lps22hb_clear_int(sensor);
    return i;
}

static rt_err_t _lps22hb_baro_init(struct rt_sensor_intf *intf)
{
    LPS22HB_IO_t io_ctx;
    rt_uint8_t        id;

    i2c_bus_dev = (struct rt_i2c_bus_device *)rt_device_find(intf->dev_name);
    if (i2c_bus_dev == RT_NULL)
    {
        return -RT_ERROR;
    }

    /* Configure the baroelero driver */
    io_ctx.BusType     = LPS22HB_I2C_BUS; /* I2C */
    io_ctx.Address     = (rt_uint32_t)(intf->user_data) & 0xff;
    io_ctx.Init        = i2c_init;
    io_ctx.DeInit      = i2c_init;
    io_ctx.ReadReg     = rt_i2c_read_reg;
    io_ctx.WriteReg    = rt_i2c_write_reg;
    io_ctx.GetTick     = lps22hb_get_tick;

    if (LPS22HB_RegisterBusIO(&lps22hb, &io_ctx) != LPS22HB_OK)
    {
        return -RT_ERROR;
    }
    else if (LPS22HB_ReadID(&lps22hb, &id) != LPS22HB_OK)
    {
        rt_kprintf("read id failed\n");
        return -RT_ERROR;
    }
    if (LPS22HB_Init(&lps22hb) != LPS22HB_OK)
    {
        rt_kprintf("lps22hb init failed\n");
        return -RT_ERROR;
    }

    return RT_EOK;
}
static rt_size_t lps22hb_fetch_data(struct rt_sensor_device *sensor, void *buf, rt_size_t len)
{
    if (sensor->config.mode == RT_SENSOR_MODE_POLLING)
    {
        return _lps22hb_polling_get_data(sensor, buf);
    }
    else if (sensor->config.mode == RT_SENSOR_MODE_INT)
    {
        return 0;
    }
    else if (sensor->config.mode == RT_SENSOR_MODE_FIFO)
    {
        return _lps22hb_fifo_get_data(sensor, buf, len);
    }
    else
        return 0;
}

static rt_err_t lps22hb_control(struct rt_sensor_device *sensor, int cmd, void *args)
{
    rt_err_t result = RT_EOK;

    switch (cmd)
    {
    case RT_SENSOR_CTRL_GET_ID:
        LPS22HB_ReadID(&lps22hb, args);
        break;
    case RT_SENSOR_CTRL_SET_RANGE:
        result = -RT_ERROR;
        break;
    case RT_SENSOR_CTRL_SET_ODR:
        result = _lps22hb_set_odr(sensor, (rt_uint32_t)args & 0xffff);
        break;
    case RT_SENSOR_CTRL_SET_MODE:
        result = _lps22hb_set_mode(sensor, (rt_uint32_t)args & 0xff);
        break;
    case RT_SENSOR_CTRL_SET_POWER:
        result = _lps22hb_set_power(sensor, (rt_uint32_t)args & 0xff);
        break;
    case RT_SENSOR_CTRL_SELF_TEST:
        break;
    default:
        return -RT_ERROR;
    }
    return result;
}

static struct rt_sensor_ops sensor_ops =
{
    lps22hb_fetch_data,
    lps22hb_control
};

int rt_hw_lps22hb_init(const char *name, struct rt_sensor_config *cfg)
{
    rt_int8_t result;
    rt_sensor_t sensor_baro = RT_NULL, sensor_temp = RT_NULL;
    struct rt_sensor_module *module = RT_NULL;
    
    module = rt_calloc(1, sizeof(struct rt_sensor_module));
    if (module == RT_NULL)
    {
        return -1;
    }
    
    {
        sensor_baro = rt_calloc(1, sizeof(struct rt_sensor_device));
        if (sensor_baro == RT_NULL)
            goto __exit;

        sensor_baro->info.type       = RT_SENSOR_CLASS_BARO;
        sensor_baro->info.vendor     = RT_SENSOR_VENDOR_STM;
        sensor_baro->info.model      = "lps22hb_baro";
        sensor_baro->info.unit       = RT_SENSOR_UNIT_MGAUSS;
        sensor_baro->info.intf_type  = RT_SENSOR_INTF_I2C;
        sensor_baro->info.period_min = 100;
        sensor_baro->info.fifo_max   = 32;

        rt_memcpy(&sensor_baro->config, cfg, sizeof(struct rt_sensor_config));
        sensor_baro->ops = &sensor_ops;
        sensor_baro->module = module;
        
        result = rt_hw_sensor_register(sensor_baro, name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_FIFO_RX, RT_NULL);
        if (result != RT_EOK)
        {
            LOG_E("device register err code: %d", result);
            goto __exit;
        }
    }
    
    {
        sensor_temp = rt_calloc(1, sizeof(struct rt_sensor_device));
        if (sensor_temp == RT_NULL)
            return -1;

        sensor_temp->info.type       = RT_SENSOR_CLASS_TEMP;
        sensor_temp->info.vendor     = RT_SENSOR_VENDOR_STM;
        sensor_temp->info.model      = "lps22hb_temp";
        sensor_temp->info.unit       = RT_SENSOR_UNIT_DCELSIUS;
        sensor_temp->info.intf_type  = RT_SENSOR_INTF_I2C;
        sensor_temp->info.period_min = 100;
        sensor_temp->info.fifo_max   = 32;

        rt_memcpy(&sensor_temp->config, cfg, sizeof(struct rt_sensor_config));
        sensor_temp->ops = &sensor_ops;
        sensor_temp->module = module;
        
        result = rt_hw_sensor_register(sensor_temp, name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_FIFO_RX, RT_NULL);
        if (result != RT_EOK)
        {
            LOG_E("device register err code: %d", result);
            goto __exit;
        }
    }
    
    module->sen[0] = sensor_baro;
    module->sen[1] = sensor_temp;
    module->sen_num = 2;

    if(_lps22hb_baro_init(&cfg->intf) != RT_EOK)
    {
        LOG_E("sensor init failed");
        goto __exit;
    }
    LPS22HB_FIFO_Set_Mode(&lps22hb, LPS22HB_FIFO_BYPASS_MODE);
    
    LOG_I("sensor init success");
    return RT_EOK;
    
__exit:
    if(sensor_baro)
    {
        rt_device_unregister(&sensor_baro->parent);
        rt_free(sensor_baro);
    }
    if(sensor_temp)
    {
        rt_device_unregister(&sensor_temp->parent);
        rt_free(sensor_temp);
    }
    if (module)
        rt_free(module);

    return -RT_ERROR;
}
