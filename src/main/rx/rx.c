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
#include "platform.h"
#include "build_config.h"
#include "debug.h"
#include "common/maths.h"
#include "config/config.h"
#include "drivers/serial.h"
#include "drivers/adc.h"
#include "io/serial.h"
#include "io/rc_controls.h"
#include "flight/failsafe.h"
#include "drivers/gpio.h"
#include "drivers/timer.h"
#include "drivers/pwm_rx.h"
#include "drivers/system.h"
#include "drivers/gyro_sync.h"
#include "rx/pwm.h"
#include "rx/sbus.h"
#include "rx/spektrum.h"
#include "rx/spektrumTelemetry.h"
#include "rx/sumd.h"
#include "rx/sumh.h"
#include "rx/msp.h"
#include "rx/xbus.h"
#include "rx/ibus.h"
#include "rx/rx.h"
void rxPwmInit(rxRuntimeConfig_t *rxRuntimeConfig, rcReadRawDataPtr *callback);
bool sbusInit(rxConfig_t *initialRxConfig, rxRuntimeConfig_t *rxRuntimeConfig, rcReadRawDataPtr *callback);
bool spektrumInit(rxConfig_t *rxConfig, rxRuntimeConfig_t *rxRuntimeConfig, rcReadRawDataPtr *callback);
bool sumdInit(rxConfig_t *rxConfig, rxRuntimeConfig_t *rxRuntimeConfig, rcReadRawDataPtr *callback);
bool sumhInit(rxConfig_t *rxConfig, rxRuntimeConfig_t *rxRuntimeConfig, rcReadRawDataPtr *callback);
bool ibusInit(rxConfig_t *rxConfig, rxRuntimeConfig_t *rxRuntimeConfig, rcReadRawDataPtr *callback);
void rxMspInit(rxConfig_t *rxConfig, rxRuntimeConfig_t *rxRuntimeConfig, rcReadRawDataPtr *callback);
const char rcChannelLetters[] = "AERT12345678abcdefgh";
uint16_t rssi = 0;
static bool rxDataReceived = false;
static bool rxSignalReceived = false;
static bool rxSignalReceivedNotDataDriven = false;
static bool rxFlightChannelsValid = false;
static bool rxIsInFailsafeMode = true;
static bool rxIsInFailsafeModeNotDataDriven = true;
bool SKIP_RX = false;
static uint32_t rxUpdateAt = 0;
static uint32_t needRxSignalBefore = 0;
static uint32_t suspendRxSignalUntil = 0;
static uint8_t skipRxSamples = 0;
int16_t rcRaw[MAX_SUPPORTED_RC_CHANNEL_COUNT];
int16_t rcData[MAX_SUPPORTED_RC_CHANNEL_COUNT];
uint32_t rcInvalidPulsPeriod[MAX_SUPPORTED_RC_CHANNEL_COUNT];
#define MAX_INVALID_PULS_TIME 300
#define PPM_AND_PWM_SAMPLE_COUNT 3
#define BASE_CHANNELS 4
#define DELAY_50_HZ (1000000 / 50)
#define DELAY_10_HZ (1000000 / 10)
#define DELAY_5_HZ (1000000 / 5)
#define SKIP_RC_ON_SUSPEND_PERIOD 1500000
#define SKIP_RC_SAMPLES_ON_RESUME 2
rxRuntimeConfig_t rxRuntimeConfig;
static rxConfig_t *rxConfig;
static uint8_t rcSampleIndex = 0;
static uint16_t nullReadRawRC(rxRuntimeConfig_t *rxRuntimeConfig, uint8_t channel) {
    UNUSED(rxRuntimeConfig);
    UNUSED(channel);
    return PPM_RCVR_TIMEOUT;
}
static rcReadRawDataPtr rcReadRawFunc = nullReadRawRC;
static uint16_t rxRefreshRate;
void serialRxInit(rxConfig_t *rxConfig);
void useRxConfig(rxConfig_t *rxConfigToUse)
{
    rxConfig = rxConfigToUse;
}
#define REQUIRED_CHANNEL_MASK 0x0F
static uint8_t validFlightChannelMask;
STATIC_UNIT_TESTED void rxResetFlightChannelStatus(void) {
    validFlightChannelMask = REQUIRED_CHANNEL_MASK;
}
STATIC_UNIT_TESTED bool rxHaveValidFlightChannels(void)
{
    return (validFlightChannelMask == REQUIRED_CHANNEL_MASK);
}
STATIC_UNIT_TESTED bool isPulseValid(uint16_t pulseDuration)
{
    return pulseDuration >= rxConfig->rx_min_usec &&
            pulseDuration <= rxConfig->rx_max_usec;
}
STATIC_UNIT_TESTED void rxUpdateFlightChannelStatus(uint8_t channel, bool valid)
{
    if (channel < NON_AUX_CHANNEL_COUNT && !valid) {
        validFlightChannelMask &= ~(1 << channel);
    }
}
void resetAllRxChannelRangeConfigurations(rxChannelRangeConfiguration_t *rxChannelRangeConfiguration) {
    for (int i = 0; i < NON_AUX_CHANNEL_COUNT; i++) {
        rxChannelRangeConfiguration->min = PWM_RANGE_MIN;
        rxChannelRangeConfiguration->max = PWM_RANGE_MAX;
        rxChannelRangeConfiguration++;
    }
}
void rxInit(rxConfig_t *rxConfig, modeActivationCondition_t *modeActivationConditions)
{
    uint8_t i;
    uint16_t value;
    useRxConfig(rxConfig);
    rcSampleIndex = 0;
    for (i = 0; i < MAX_SUPPORTED_RC_CHANNEL_COUNT; i++) {
        rcData[i] = rxConfig->midrc;
        rcInvalidPulsPeriod[i] = millis() + MAX_INVALID_PULS_TIME;
    }
    rcData[THROTTLE] = (feature(FEATURE_3D)) ? rxConfig->midrc : rxConfig->rx_min_usec;
    for (i = 0; i < MAX_MODE_ACTIVATION_CONDITION_COUNT; i++) {
        modeActivationCondition_t *modeActivationCondition = &modeActivationConditions[i];
        if (modeActivationCondition->modeId == BOXARM && IS_RANGE_USABLE(&modeActivationCondition->range)) {
            if (modeActivationCondition->range.startStep > 0) {
                value = MODE_STEP_TO_CHANNEL_VALUE((modeActivationCondition->range.startStep - 1));
            } else {
                value = MODE_STEP_TO_CHANNEL_VALUE((modeActivationCondition->range.endStep + 1));
            }
            rcData[modeActivationCondition->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = value;
        }
    }
#ifdef SERIAL_RX
    if (feature(FEATURE_RX_SERIAL)) {
        serialRxInit(rxConfig);
    }
#endif
    if (feature(FEATURE_RX_MSP)) {
        rxMspInit(rxConfig, &rxRuntimeConfig, &rcReadRawFunc);
    }
    if (feature(FEATURE_RX_PPM) || feature(FEATURE_RX_PARALLEL_PWM)) {
        rxRefreshRate = 20000;
        rxPwmInit(&rxRuntimeConfig, &rcReadRawFunc);
    }
    rxRuntimeConfig.auxChannelCount = rxRuntimeConfig.channelCount - STICK_CHANNEL_COUNT;
}
#ifdef SERIAL_RX
void serialRxInit(rxConfig_t *rxConfig)
{
    bool enabled = false;
    switch (rxConfig->serialrx_provider) {
        case SERIALRX_SPEKTRUM1024:
            rxRefreshRate = 22000;
            enabled = spektrumInit(rxConfig, &rxRuntimeConfig, &rcReadRawFunc);
            break;
        case SERIALRX_SPEKTRUM2048:
            rxRefreshRate = 11000;
            enabled = spektrumInit(rxConfig, &rxRuntimeConfig, &rcReadRawFunc);
            break;
        case SERIALRX_SBUS:
            rxRefreshRate = 9000;
            enabled = sbusInit(rxConfig, &rxRuntimeConfig, &rcReadRawFunc);
            break;
        case SERIALRX_SUMD:
            rxRefreshRate = 11000;
            enabled = sumdInit(rxConfig, &rxRuntimeConfig, &rcReadRawFunc);
            break;
        case SERIALRX_SUMH:
            rxRefreshRate = 11000;
            enabled = sumhInit(rxConfig, &rxRuntimeConfig, &rcReadRawFunc);
            break;
        case SERIALRX_XBUS_MODE_B:
        case SERIALRX_XBUS_MODE_B_RJ01:
            rxRefreshRate = 11000;
            enabled = xBusInit(rxConfig, &rxRuntimeConfig, &rcReadRawFunc);
            break;
        case SERIALRX_IBUS:
            rxRefreshRate = 11000;
            enabled = ibusInit(rxConfig, &rxRuntimeConfig, &rcReadRawFunc);
            break;
    }
    if (!enabled) {
        featureClear(FEATURE_RX_SERIAL);
        rcReadRawFunc = nullReadRawRC;
    }
}
uint8_t serialRxFrameStatus(rxConfig_t *rxConfig)
{
    switch (rxConfig->serialrx_provider) {
        case SERIALRX_SPEKTRUM1024:
        case SERIALRX_SPEKTRUM2048:
   return spektrumFrameStatus(rxConfig, &rxRuntimeConfig);
        case SERIALRX_SBUS:
            return sbusFrameStatus();
        case SERIALRX_SUMD:
            return sumdFrameStatus();
        case SERIALRX_SUMH:
            return sumhFrameStatus();
        case SERIALRX_XBUS_MODE_B:
        case SERIALRX_XBUS_MODE_B_RJ01:
            return xBusFrameStatus();
        case SERIALRX_IBUS:
            return ibusFrameStatus();
    }
    return SERIAL_RX_FRAME_PENDING;
}
#endif
uint8_t calculateChannelRemapping(uint8_t *channelMap, uint8_t channelMapEntryCount, uint8_t channelToRemap)
{
    if (channelToRemap < channelMapEntryCount) {
        return channelMap[channelToRemap];
    }
    return channelToRemap;
}
bool rxIsReceivingSignal(void)
{
    return rxSignalReceived;
}
bool rxAreFlightChannelsValid(void)
{
    return rxFlightChannelsValid;
}
static bool isRxDataDriven(void) {
    return !(feature(FEATURE_RX_PARALLEL_PWM | FEATURE_RX_PPM));
}
static void resetRxSignalReceivedFlagIfNeeded(uint32_t currentTime)
{
    if (!rxSignalReceived) {
        return;
    }
    if (((int32_t)(currentTime - needRxSignalBefore) >= 0)) {
        rxSignalReceived = false;
        rxSignalReceivedNotDataDriven = false;
    }
}
void suspendRxSignal(void)
{
    suspendRxSignalUntil = micros() + SKIP_RC_ON_SUSPEND_PERIOD;
    skipRxSamples = SKIP_RC_SAMPLES_ON_RESUME;
    failsafeOnRxSuspend(SKIP_RC_ON_SUSPEND_PERIOD);
}
void resumeRxSignal(void)
{
    suspendRxSignalUntil = micros();
    skipRxSamples = SKIP_RC_SAMPLES_ON_RESUME;
    failsafeOnRxResume();
}
void updateRx(uint32_t currentTime)
{
    resetRxSignalReceivedFlagIfNeeded(currentTime);
    if (isRxDataDriven()) {
        rxDataReceived = false;
    }
    if (feature(FEATURE_RX_SERIAL)) {
        uint8_t frameStatus = serialRxFrameStatus(rxConfig);
        if (frameStatus & SERIAL_RX_FRAME_COMPLETE) {
            rxDataReceived = true;
            rxIsInFailsafeMode = (frameStatus & SERIAL_RX_FRAME_FAILSAFE) != 0;
            rxSignalReceived = !rxIsInFailsafeMode;
            needRxSignalBefore = currentTime + DELAY_10_HZ;
        }
    }
    if (feature(FEATURE_RX_MSP)) {
        rxDataReceived = rxMspFrameComplete();
        if (rxDataReceived) {
            rxSignalReceived = true;
            rxIsInFailsafeMode = false;
            needRxSignalBefore = currentTime + DELAY_5_HZ;
        }
    }
    if (feature(FEATURE_RX_PPM)) {
        if (isPPMDataBeingReceived()) {
            rxSignalReceivedNotDataDriven = true;
            rxIsInFailsafeModeNotDataDriven = false;
            needRxSignalBefore = currentTime + DELAY_10_HZ;
            resetPPMDataReceivedState();
        }
    }
    if (feature(FEATURE_RX_PARALLEL_PWM)) {
        if (isPWMDataBeingReceived()) {
            rxSignalReceivedNotDataDriven = true;
            rxIsInFailsafeModeNotDataDriven = false;
            needRxSignalBefore = currentTime + DELAY_10_HZ;
        }
    }
}
bool shouldProcessRx(uint32_t currentTime)
{
    return rxDataReceived || ((int32_t)(currentTime - rxUpdateAt) >= 0);
}
static uint16_t calculateNonDataDrivenChannel(uint8_t chan, uint16_t sample)
{
    static int16_t rcSamples[MAX_SUPPORTED_RX_PARALLEL_PWM_OR_PPM_CHANNEL_COUNT][PPM_AND_PWM_SAMPLE_COUNT];
    static int16_t rcDataMean[MAX_SUPPORTED_RX_PARALLEL_PWM_OR_PPM_CHANNEL_COUNT];
    static bool rxSamplesCollected = false;
    uint8_t currentSampleIndex = rcSampleIndex % PPM_AND_PWM_SAMPLE_COUNT;
    rcSamples[chan][currentSampleIndex] = sample;
    if (!rxSamplesCollected) {
        if (rcSampleIndex < PPM_AND_PWM_SAMPLE_COUNT) {
            return sample;
        }
        rxSamplesCollected = true;
    }
    rcDataMean[chan] = 0;
    uint8_t sampleIndex;
    for (sampleIndex = 0; sampleIndex < PPM_AND_PWM_SAMPLE_COUNT; sampleIndex++)
        rcDataMean[chan] += rcSamples[chan][sampleIndex];
    return rcDataMean[chan] / PPM_AND_PWM_SAMPLE_COUNT;
}
static uint16_t getRxfailValue(uint8_t channel)
{
    rxFailsafeChannelConfiguration_t *channelFailsafeConfiguration = &rxConfig->failsafe_channel_configurations[channel];
    switch(channelFailsafeConfiguration->mode) {
        case RX_FAILSAFE_MODE_AUTO:
            switch (channel) {
                case ROLL:
                case PITCH:
                case YAW:
                    return rxConfig->midrc;
                case THROTTLE:
                    if (feature(FEATURE_3D))
                        return rxConfig->midrc;
                    else
                        return rxConfig->rx_min_usec;
            }
        default:
        case RX_FAILSAFE_MODE_INVALID:
        case RX_FAILSAFE_MODE_HOLD:
            return rcData[channel];
        case RX_FAILSAFE_MODE_SET:
            return RXFAIL_STEP_TO_CHANNEL_VALUE(channelFailsafeConfiguration->step);
    }
}
STATIC_UNIT_TESTED uint16_t applyRxChannelRangeConfiguraton(int sample, rxChannelRangeConfiguration_t range)
{
    if (sample == PPM_RCVR_TIMEOUT) {
        return PPM_RCVR_TIMEOUT;
    }
    sample = scaleRange(sample, range.min, range.max, PWM_RANGE_MIN, PWM_RANGE_MAX);
    sample = MIN(MAX(PWM_PULSE_MIN, sample), PWM_PULSE_MAX);
    return sample;
}
static uint8_t getRxChannelCount(void) {
    static uint8_t maxChannelsAllowed;
    if (!maxChannelsAllowed) {
        if (BASE_CHANNELS + rxConfig->max_aux_channels > rxRuntimeConfig.channelCount) {
            maxChannelsAllowed = rxRuntimeConfig.channelCount;
        } else {
            maxChannelsAllowed = BASE_CHANNELS + rxConfig->max_aux_channels;
        }
    }
    return maxChannelsAllowed;
}
static void readRxChannelsApplyRanges(void)
{
    uint8_t channel;
    for (channel = 0; channel < getRxChannelCount(); channel++) {
        uint8_t rawChannel = calculateChannelRemapping(rxConfig->rcmap, REMAPPABLE_CHANNEL_COUNT, channel);
        uint16_t sample = rcReadRawFunc(&rxRuntimeConfig, rawChannel);
        if (channel < NON_AUX_CHANNEL_COUNT) {
            sample = applyRxChannelRangeConfiguraton(sample, rxConfig->channelRanges[channel]);
        }
        rcRaw[channel] = sample;
    }
}
#undef DEBUG_RX_SIGNAL_LOSS
static void detectAndApplySignalLossBehaviour(void)
{
    int channel;
    uint16_t sample;
    bool useValueFromRx = true;
    bool rxIsDataDriven = isRxDataDriven();
    uint32_t currentMilliTime = millis();
    if (!rxIsDataDriven) {
        rxSignalReceived = rxSignalReceivedNotDataDriven;
        rxIsInFailsafeMode = rxIsInFailsafeModeNotDataDriven;
    }
    if (!rxSignalReceived || rxIsInFailsafeMode) {
        useValueFromRx = false;
    }
#ifdef DEBUG_RX_SIGNAL_LOSS
    debug[0] = rxSignalReceived;
    debug[1] = rxIsInFailsafeMode;
    debug[2] = rcReadRawFunc(&rxRuntimeConfig, 0);
#endif
    rxResetFlightChannelStatus();
    for (channel = 0; channel < getRxChannelCount(); channel++) {
        sample = (useValueFromRx) ? rcRaw[channel] : PPM_RCVR_TIMEOUT;
        bool validPulse = isPulseValid(sample);
        if (!validPulse) {
            if (currentMilliTime < rcInvalidPulsPeriod[channel]) {
                sample = rcData[channel];
            } else {
                sample = getRxfailValue(channel);
                rxUpdateFlightChannelStatus(channel, validPulse);
            }
        } else {
            rcInvalidPulsPeriod[channel] = currentMilliTime + MAX_INVALID_PULS_TIME;
        }
        if (rxIsDataDriven) {
            rcData[channel] = sample;
        } else {
            rcData[channel] = calculateNonDataDrivenChannel(channel, sample);
        }
    }
    rxFlightChannelsValid = rxHaveValidFlightChannels();
    if ((rxFlightChannelsValid) && !IS_RC_MODE_ACTIVE(BOXFAILSAFE)) {
        failsafeOnValidDataReceived();
    } else {
        rxIsInFailsafeMode = rxIsInFailsafeModeNotDataDriven = true;
        failsafeOnValidDataFailed();
        for (channel = 0; channel < getRxChannelCount(); channel++) {
            rcData[channel] = getRxfailValue(channel);
        }
    }
#ifdef DEBUG_RX_SIGNAL_LOSS
    debug[3] = rcData[THROTTLE];
#endif
}
void calculateRxChannelsAndUpdateFailsafe(uint32_t currentTime)
{
    rxUpdateAt = currentTime + DELAY_50_HZ;
    if (skipRxSamples) {
        if (currentTime > suspendRxSignalUntil) {
            skipRxSamples--;
        }
        return;
    }
    readRxChannelsApplyRanges();
    detectAndApplySignalLossBehaviour();
    rcSampleIndex++;
}
void parseRcChannels(const char *input, rxConfig_t *rxConfig)
{
    const char *c, *s;
    for (c = input; *c; c++) {
        s = strchr(rcChannelLetters, *c);
        if (s && (s < rcChannelLetters + MAX_MAPPABLE_RX_INPUTS))
            rxConfig->rcmap[s - rcChannelLetters] = c - input;
    }
}
void updateRSSIPWM(void)
{
    int16_t pwmRssi = 0;
    pwmRssi = rcData[rxConfig->rssi_channel - 1];
 if (rxConfig->rssi_ppm_invert) {
     pwmRssi = ((2000 - pwmRssi) + 1000);
 }
    rssi = (uint16_t)((constrain(pwmRssi - 1000, 0, 1000) / 1000.0f) * 1023.0f);
}
#define RSSI_ADC_SAMPLE_COUNT 16
void updateRSSIADC(uint32_t currentTime)
{
#ifndef USE_ADC
    UNUSED(currentTime);
#else
    static uint8_t adcRssiSamples[RSSI_ADC_SAMPLE_COUNT];
    static uint8_t adcRssiSampleIndex = 0;
    static uint32_t rssiUpdateAt = 0;
    if ((int32_t)(currentTime - rssiUpdateAt) < 0) {
        return;
    }
    rssiUpdateAt = currentTime + DELAY_50_HZ;
    int16_t adcRssiMean = 0;
    uint16_t adcRssiSample = adcGetChannel(ADC_RSSI);
    uint8_t rssiPercentage = adcRssiSample / rxConfig->rssi_scale;
    adcRssiSampleIndex = (adcRssiSampleIndex + 1) % RSSI_ADC_SAMPLE_COUNT;
    adcRssiSamples[adcRssiSampleIndex] = rssiPercentage;
    uint8_t sampleIndex;
    for (sampleIndex = 0; sampleIndex < RSSI_ADC_SAMPLE_COUNT; sampleIndex++) {
        adcRssiMean += adcRssiSamples[sampleIndex];
    }
    adcRssiMean = adcRssiMean / RSSI_ADC_SAMPLE_COUNT;
    rssi = (uint16_t)((constrain(adcRssiMean, 0, 100) / 100.0f) * 1023.0f);
#endif
}
void updateRSSI(uint32_t currentTime)
{
    if (rxConfig->rssi_channel > 0) {
        updateRSSIPWM();
    } else if (feature(FEATURE_RSSI_ADC)) {
        updateRSSIADC(currentTime);
    }
}
void initRxRefreshRate(uint16_t *rxRefreshRatePtr) {
    *rxRefreshRatePtr = rxRefreshRate;
}
