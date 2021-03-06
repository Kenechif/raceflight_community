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
#include <stdlib.h>
#include "platform.h"
#include "gpio.h"
#include "system.h"
#define AIRCR_VECTKEY_MASK ((uint32_t)0x05FA0000)
void systemReset(void)
{
    SCB->AIRCR = AIRCR_VECTKEY_MASK | (uint32_t)0x04;
}
void systemResetToBootloader(void) {
    *((uint32_t *)0x20009FFC) = 0xDEADBEEF;
    systemReset();
}
void systemResetToDFUloader(void) {
 systemResetToBootloader();
}
void enableGPIOPowerUsageAndNoiseReductions(void)
{
    RCC_AHBPeriphClockCmd(
        RCC_AHBPeriph_GPIOA |
        RCC_AHBPeriph_GPIOB |
        RCC_AHBPeriph_GPIOC |
        RCC_AHBPeriph_GPIOD |
        RCC_AHBPeriph_GPIOE |
        RCC_AHBPeriph_GPIOF,
        ENABLE
    );
    gpio_config_t gpio;
    gpio.mode = Mode_AIN;
    gpio.pin = Pin_All & ~(Pin_13 | Pin_14 | Pin_15);
    gpioInit(GPIOA, &gpio);
    gpio.pin = Pin_All;
    gpioInit(GPIOB, &gpio);
    gpioInit(GPIOC, &gpio);
    gpioInit(GPIOD, &gpio);
    gpioInit(GPIOE, &gpio);
    gpioInit(GPIOF, &gpio);
}
bool isMPUSoftReset(void)
{
    if (cachedRccCsrValue & RCC_CSR_SFTRSTF)
        return true;
    else
        return false;
}
