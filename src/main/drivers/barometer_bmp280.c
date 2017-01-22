/* 
 * This file is part of RaceFlight. 
 * 
 * RaceFlight is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version. 
 * 
 * RaceFlight is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * along with RaceFlight.  If not, see <http://www.gnu.org/licenses/>.
 * You should have received a copy of the GNU General Public License 
 * along with RaceFlight.  If not, see <http://www.gnu.org/licenses/>.
 */ 
#include <stdbool.h>
#include <stdint.h>
#include <platform.h>
#include "build_config.h"
#include "barometer.h"
#include "system.h"
#include "bus_i2c.h"
#include "barometer_bmp280.h"
#ifdef BARO
#ifndef BMP280_I2C_INSTANCE
#define BMP280_I2C_INSTANCE I2C_DEVICE
#endif
#define BMP280_I2C_ADDR (0x76)
#define BMP280_DEFAULT_CHIP_ID (0x58)
#define BMP280_CHIP_ID_REG (0xD0)
#define BMP280_RST_REG (0xE0)
#define BMP280_STAT_REG (0xF3)
#define BMP280_CTRL_MEAS_REG (0xF4)
#define BMP280_CONFIG_REG (0xF5)
#define BMP280_PRESSURE_MSB_REG (0xF7)
#define BMP280_PRESSURE_LSB_REG (0xF8)
#define BMP280_PRESSURE_XLSB_REG (0xF9)
#define BMP280_TEMPERATURE_MSB_REG (0xFA)
#define BMP280_TEMPERATURE_LSB_REG (0xFB)
#define BMP280_TEMPERATURE_XLSB_REG (0xFC)
#define BMP280_FORCED_MODE (0x01)
#define BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG (0x88)
#define BMP280_PRESSURE_TEMPERATURE_CALIB_DATA_LENGTH (24)
#define BMP280_DATA_FRAME_SIZE (6)
#define BMP280_OVERSAMP_SKIPPED (0x00)
#define BMP280_OVERSAMP_1X (0x01)
#define BMP280_OVERSAMP_2X (0x02)
#define BMP280_OVERSAMP_4X (0x03)
#define BMP280_OVERSAMP_8X (0x04)
#define BMP280_OVERSAMP_16X (0x05)
#define BMP280_PRESSURE_OSR (BMP280_OVERSAMP_8X)
#define BMP280_TEMPERATURE_OSR (BMP280_OVERSAMP_1X)
#define BMP280_MODE (BMP280_PRESSURE_OSR << 2 | BMP280_TEMPERATURE_OSR << 5 | BMP280_FORCED_MODE)
#define T_INIT_MAX (20)
#define T_MEASURE_PER_OSRS_MAX (37)
#define T_SETUP_PRESSURE_MAX (10)
typedef struct bmp280_calib_param_s {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
    int32_t t_fine;
} bmp280_calib_param_t;
static uint8_t bmp280_chip_id = 0;
static bool bmp280InitDone = false;
STATIC_UNIT_TESTED bmp280_calib_param_t bmp280_cal;
STATIC_UNIT_TESTED int32_t bmp280_up = 0;
STATIC_UNIT_TESTED int32_t bmp280_ut = 0;
static void bmp280_start_ut(void);
static void bmp280_get_ut(void);
static void bmp280_start_up(void);
static void bmp280_get_up(void);
STATIC_UNIT_TESTED void bmp280_calculate(int32_t *pressure, int32_t *temperature);
bool bmp280Detect(baro_t *baro)
{
    if (bmp280InitDone)
        return true;
    delay(20);
    i2cRead(BMP280_I2C_INSTANCE, BMP280_I2C_ADDR, BMP280_CHIP_ID_REG, 1, &bmp280_chip_id);
    if (bmp280_chip_id != BMP280_DEFAULT_CHIP_ID)
        return false;
    i2cRead(BMP280_I2C_INSTANCE, BMP280_I2C_ADDR, BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG, 24, (uint8_t *)&bmp280_cal);
    i2cWrite(BMP280_I2C_INSTANCE, BMP280_I2C_ADDR, BMP280_CTRL_MEAS_REG, BMP280_MODE);
    bmp280InitDone = true;
    baro->ut_delay = 0;
    baro->get_ut = bmp280_get_ut;
    baro->start_ut = bmp280_start_ut;
    baro->up_delay = ((T_INIT_MAX + T_MEASURE_PER_OSRS_MAX * (((1 << BMP280_TEMPERATURE_OSR) >> 1) + ((1 << BMP280_PRESSURE_OSR) >> 1)) + (BMP280_PRESSURE_OSR ? T_SETUP_PRESSURE_MAX : 0) + 15) / 16) * 1000;
    baro->start_up = bmp280_start_up;
    baro->get_up = bmp280_get_up;
    baro->calculate = bmp280_calculate;
    return true;
}
static void bmp280_start_ut(void)
{
}
static void bmp280_get_ut(void)
{
}
static void bmp280_start_up(void)
{
    i2cWrite(BMP280_I2C_INSTANCE, BMP280_I2C_ADDR, BMP280_CTRL_MEAS_REG, BMP280_MODE);
}
static void bmp280_get_up(void)
{
    uint8_t data[BMP280_DATA_FRAME_SIZE];
    i2cRead(BMP280_I2C_INSTANCE, BMP280_I2C_ADDR, BMP280_PRESSURE_MSB_REG, BMP280_DATA_FRAME_SIZE, data);
    bmp280_up = (int32_t)((((uint32_t)(data[0])) << 12) | (((uint32_t)(data[1])) << 4) | ((uint32_t)data[2] >> 4));
    bmp280_ut = (int32_t)((((uint32_t)(data[3])) << 12) | (((uint32_t)(data[4])) << 4) | ((uint32_t)data[5] >> 4));
}
static int32_t bmp280_compensate_T(int32_t adc_T)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)bmp280_cal.dig_T1 << 1))) * ((int32_t)bmp280_cal.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)bmp280_cal.dig_T1)) * ((adc_T >> 4) - ((int32_t)bmp280_cal.dig_T1))) >> 12) * ((int32_t)bmp280_cal.dig_T3)) >> 14;
    bmp280_cal.t_fine = var1 + var2;
    T = (bmp280_cal.t_fine * 5 + 128) >> 8;
    return T;
}
static uint32_t bmp280_compensate_P(int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)bmp280_cal.t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bmp280_cal.dig_P6;
    var2 = var2 + ((var1*(int64_t)bmp280_cal.dig_P5) << 17);
    var2 = var2 + (((int64_t)bmp280_cal.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bmp280_cal.dig_P3) >> 8) + ((var1 * (int64_t)bmp280_cal.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmp280_cal.dig_P1) >> 33;
    if (var1 == 0)
        return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bmp280_cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)bmp280_cal.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)bmp280_cal.dig_P7) << 4);
    return (uint32_t)p;
}
STATIC_UNIT_TESTED void bmp280_calculate(int32_t *pressure, int32_t *temperature)
{
    int32_t t;
    uint32_t p;
    t = bmp280_compensate_T(bmp280_ut);
    p = bmp280_compensate_P(bmp280_up);
    if (pressure)
        *pressure = (int32_t)(p / 256);
    if (temperature)
        *temperature = t;
}
#endif
