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
#include <string.h>
#include "build_config.h"
#include "platform.h"
#include "drivers/gpio.h"
#include "drivers/timer.h"
#include "drivers/pwm_rx.h"
#include "config/config.h"
#include "rx/rx.h"
#include "rx/pwm.h"
#include "watchdog.h"
static uint16_t pwmReadRawRC(rxRuntimeConfig_t *rxRuntimeConfigPtr, uint8_t channel)
{
    UNUSED(rxRuntimeConfigPtr);
    resetTimeSinceRxPulse();
    return pwmRead(channel);
}
static uint16_t ppmReadRawRC(rxRuntimeConfig_t *rxRuntimeConfigPtr, uint8_t channel)
{
    UNUSED(rxRuntimeConfigPtr);
    resetTimeSinceRxPulse();
    return ppmRead(channel);
}
void rxPwmInit(rxRuntimeConfig_t *rxRuntimeConfigPtr, rcReadRawDataPtr *callback)
{
    UNUSED(rxRuntimeConfigPtr);
    if (feature(FEATURE_RX_PARALLEL_PWM)) {
        rxRuntimeConfigPtr->channelCount = MAX_SUPPORTED_RC_PARALLEL_PWM_CHANNEL_COUNT;
        *callback = pwmReadRawRC;
    }
    if (feature(FEATURE_RX_PPM)) {
        rxRuntimeConfigPtr->channelCount = MAX_SUPPORTED_RC_PPM_CHANNEL_COUNT;
        *callback = ppmReadRawRC;
    }
}
