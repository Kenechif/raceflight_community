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
#include "platform.h"
#include "common/maths.h"
#include "common/axis.h"
#include "common/color.h"
#include "common/utils.h"
#include "drivers/gpio.h"
#include "drivers/sensor.h"
#include "drivers/system.h"
#include "drivers/serial.h"
#include "drivers/compass.h"
#include "drivers/timer.h"
#include "drivers/pwm_rx.h"
#include "drivers/accgyro.h"
#include "drivers/light_led.h"
#include "drivers/sound_beeper.h"
#include "sensors/sensors.h"
#include "sensors/boardalignment.h"
#include "sensors/sonar.h"
#include "sensors/compass.h"
#include "sensors/acceleration.h"
#include "sensors/barometer.h"
#include "sensors/gyro.h"
#include "sensors/battery.h"
#include "io/display.h"
#include "io/escservo.h"
#include "io/rc_controls.h"
#include "io/gimbal.h"
#include "io/gps.h"
#include "io/ledstrip.h"
#include "io/serial.h"
#include "io/serial_cli.h"
#include "io/serial_msp.h"
#include "io/statusindicator.h"
#include "rx/rx.h"
#include "rx/msp.h"
#include "telemetry/telemetry.h"
#include "flight/mixer.h"
#include "flight/altitudehold.h"
#include "flight/failsafe.h"
#include "flight/imu.h"
#include "flight/navigation.h"
#include "io/beeper.h"
#include "config/runtime_config.h"
#include "config/config.h"
#include "config/config_profile.h"
#include "config/config_master.h"
#if FLASH_SIZE > 64
#define BEEPER_NAMES 
#endif
#define MAX_MULTI_BEEPS 20
#define BEEPER_COMMAND_REPEAT 0xFE
#define BEEPER_COMMAND_STOP 0xFF
static const uint8_t beep_shortBeep[] = {
    10, 10, BEEPER_COMMAND_STOP
};
static const uint8_t beep_armingBeep[] = {
    30, 5, 5, 5, BEEPER_COMMAND_STOP
};
static const uint8_t beep_armedBeep[] = {
    0, 245, 10, 5, BEEPER_COMMAND_STOP
};
static const uint8_t beep_disarmBeep[] = {
    15, 5, 15, 5, BEEPER_COMMAND_STOP
};
static const uint8_t beep_disarmRepeatBeep[] = {
    0, 100, 10, BEEPER_COMMAND_STOP
};
static const uint8_t beep_lowBatteryBeep[] = {
    25, 50, BEEPER_COMMAND_STOP
};
static const uint8_t beep_critBatteryBeep[] = {
    50, 2, BEEPER_COMMAND_STOP
};
static const uint8_t beep_txLostBeep[] = {
    50, 50, BEEPER_COMMAND_STOP
};
static const uint8_t beep_sos[] = {
  70
};
static const uint8_t beep_armedGpsFix[] = {
    5, 5, 15, 5, 5, 5, 15, 30, BEEPER_COMMAND_STOP
};
static const uint8_t beep_readyBeep[] = {
    4, 5, 4, 5, 8, 5, 15, 5, 8, 5, 4, 5, 4, 5, BEEPER_COMMAND_STOP
};
static const uint8_t beep_2shortBeeps[] = {
    5, 5, 5, 5, BEEPER_COMMAND_STOP
};
static const uint8_t beep_2longerBeeps[] = {
    20, 15, 35, 5, BEEPER_COMMAND_STOP
};
static const uint8_t beep_gyroCalibrated[] = {
    20, 10, 20, 10, 20, 10, BEEPER_COMMAND_STOP
};
static uint8_t beep_multiBeeps[MAX_MULTI_BEEPS + 2];
#define BEEPER_CONFIRMATION_BEEP_DURATION 2
#define BEEPER_CONFIRMATION_BEEP_GAP_DURATION 20
static uint8_t beeperIsOn = 0;
static uint16_t beeperPos = 0;
static uint32_t beeperNextToggleTime = 0;
static uint32_t armingBeepTimeMicros = 0;
static void beeperProcessCommand(void);
typedef struct beeperTableEntry_s {
    uint8_t mode;
    uint8_t priority;
    const uint8_t *sequence;
#ifdef BEEPER_NAMES
    const char *name;
#endif
} beeperTableEntry_t;
#ifdef BEEPER_NAMES
#define BEEPER_ENTRY(a,b,c,d) a,b,c,d
#else
#define BEEPER_ENTRY(a,b,c,d) a,b,c
#endif
static const beeperTableEntry_t beeperTable[] = {
    { BEEPER_ENTRY(BEEPER_GYRO_CALIBRATED, 0, beep_gyroCalibrated, "GYRO_CALIBRATED") },
    { BEEPER_ENTRY(BEEPER_RX_LOST_LANDING, 1, beep_sos, "RX_LOST_LANDING") },
    { BEEPER_ENTRY(BEEPER_RX_LOST, 2, beep_txLostBeep, "RX_LOST") },
    { BEEPER_ENTRY(BEEPER_DISARMING, 3, beep_disarmBeep, "DISARMING") },
    { BEEPER_ENTRY(BEEPER_ARMING, 4, beep_armingBeep, "ARMING") },
    { BEEPER_ENTRY(BEEPER_ARMING_GPS_FIX, 5, beep_armedGpsFix, "ARMING_GPS_FIX") },
    { BEEPER_ENTRY(BEEPER_BAT_CRIT_LOW, 6, beep_critBatteryBeep, "BAT_CRIT_LOW") },
    { BEEPER_ENTRY(BEEPER_BAT_LOW, 7, beep_lowBatteryBeep, "BAT_LOW") },
    { BEEPER_ENTRY(BEEPER_USB, 8, beep_multiBeeps, NULL) },
    { BEEPER_ENTRY(BEEPER_RX_SET, 9, beep_shortBeep, "RX_SET") },
    { BEEPER_ENTRY(BEEPER_ACC_CALIBRATION, 10, beep_2shortBeeps, "ACC_CALIBRATION") },
    { BEEPER_ENTRY(BEEPER_ACC_CALIBRATION_FAIL, 11, beep_2longerBeeps, "ACC_CALIBRATION_FAIL") },
    { BEEPER_ENTRY(BEEPER_READY_BEEP, 12, beep_readyBeep, "READY_BEEP") },
    { BEEPER_ENTRY(BEEPER_MULTI_BEEPS, 13, beep_multiBeeps, NULL) },
    { BEEPER_ENTRY(BEEPER_DISARM_REPEAT, 14, beep_disarmRepeatBeep, "DISARM_REPEAT") },
    { BEEPER_ENTRY(BEEPER_ARMED, 15, beep_armedBeep, "ARMED") },
};
static const beeperTableEntry_t *currentBeeperEntry = NULL;
#define BEEPER_TABLE_ENTRY_COUNT (sizeof(beeperTable) / sizeof(beeperTableEntry_t))
void beeper(beeperMode_e mode)
{
 if (mode == BEEPER_SILENCE || ((masterConfig.beeper_off.flags & (1 << (BEEPER_USB - 1))) && (feature(FEATURE_VBAT) && (batteryCellCount < 2)))) {
        beeperSilence();
        return;
    }
    const beeperTableEntry_t *selectedCandidate = NULL;
    for (uint32_t i = 0; i < BEEPER_TABLE_ENTRY_COUNT; i++) {
        const beeperTableEntry_t *candidate = &beeperTable[i];
        if (candidate->mode != mode) {
            continue;
        }
        if (!currentBeeperEntry) {
            selectedCandidate = candidate;
            break;
        }
        if (candidate->priority < currentBeeperEntry->priority) {
            selectedCandidate = candidate;
        }
        break;
    }
    if (!selectedCandidate) {
        return;
    }
    currentBeeperEntry = selectedCandidate;
    beeperPos = 0;
    beeperNextToggleTime = 0;
}
void beeperSilence(void)
{
    BEEP_OFF;
    warningLedDisable();
    warningLedRefresh();
    beeperIsOn = 0;
    beeperNextToggleTime = 0;
    beeperPos = 0;
    currentBeeperEntry = NULL;
}
void beeperConfirmationBeeps(uint8_t beepCount)
{
    int i;
    int cLimit;
    i = 0;
    cLimit = beepCount * 2;
    if(cLimit > MAX_MULTI_BEEPS)
        cLimit = MAX_MULTI_BEEPS;
    do {
        beep_multiBeeps[i++] = BEEPER_CONFIRMATION_BEEP_DURATION;
        beep_multiBeeps[i++] = BEEPER_CONFIRMATION_BEEP_GAP_DURATION;
    } while (i < cLimit);
    beep_multiBeeps[i] = BEEPER_COMMAND_STOP;
    beeper(BEEPER_MULTI_BEEPS);
}
#ifdef GPS
void beeperGpsStatus(void)
{
    if (STATE(GPS_FIX) && GPS_numSat >= 5) {
        uint8_t i = 0;
        do {
            beep_multiBeeps[i++] = 5;
            beep_multiBeeps[i++] = 10;
        } while (i < MAX_MULTI_BEEPS && GPS_numSat > i / 2);
        beep_multiBeeps[i-1] = 50;
        beep_multiBeeps[i] = BEEPER_COMMAND_STOP;
        beeper(BEEPER_MULTI_BEEPS);
    } else {
        beeper(BEEPER_RX_SET);
    }
}
#endif
void beeperUpdate(void)
{
    if (IS_RC_MODE_ACTIVE(BOXBEEPERON)) {
#ifdef GPS
        if (feature(FEATURE_GPS)) {
            beeperGpsStatus();
        } else {
            beeper(BEEPER_RX_SET);
        }
#else
        beeper(BEEPER_RX_SET);
#endif
    }
    if (currentBeeperEntry == NULL) {
        return;
    }
    uint32_t now = millis();
    if (beeperNextToggleTime > now) {
        return;
    }
    if (!beeperIsOn) {
        beeperIsOn = 1;
        if (currentBeeperEntry->sequence[beeperPos] != 0) {
         if (!(masterConfig.beeper_off.flags & (1 << (currentBeeperEntry->mode - 1))))
                BEEP_ON;
            warningLedEnable();
            warningLedRefresh();
            if (
                beeperPos == 0
                && (currentBeeperEntry->mode == BEEPER_ARMING || currentBeeperEntry->mode == BEEPER_ARMING_GPS_FIX)
            ) {
                armingBeepTimeMicros = micros();
            }
        }
    } else {
        beeperIsOn = 0;
        if (currentBeeperEntry->sequence[beeperPos] != 0) {
            BEEP_OFF;
            warningLedDisable();
            warningLedRefresh();
        }
    }
    beeperProcessCommand();
}
static void beeperProcessCommand(void)
{
    if (currentBeeperEntry->sequence[beeperPos] == BEEPER_COMMAND_REPEAT) {
        beeperPos = 0;
    } else if (currentBeeperEntry->sequence[beeperPos] == BEEPER_COMMAND_STOP) {
        beeperSilence();
    } else {
        beeperNextToggleTime = millis() + 10 * currentBeeperEntry->sequence[beeperPos];
        beeperPos++;
    }
}
uint32_t getArmingBeepTimeMicros(void)
{
    return armingBeepTimeMicros;
}
beeperMode_e beeperModeForTableIndex(int idx)
{
    return (idx >= 0 && idx < (int)BEEPER_TABLE_ENTRY_COUNT) ? beeperTable[idx].mode : BEEPER_SILENCE;
}
const char *beeperNameForTableIndex(int idx)
{
#ifndef BEEPER_NAMES
    UNUSED(idx);
    return NULL;
#else
    return (idx >= 0 && idx < (int)BEEPER_TABLE_ENTRY_COUNT) ? beeperTable[idx].name : NULL;
#endif
}
int beeperTableEntryCount(void)
{
    return (int)BEEPER_TABLE_ENTRY_COUNT;
}
