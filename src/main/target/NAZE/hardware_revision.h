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
 */ 
#pragma once 
       
typedef enum nazeHardwareRevision_t {
    UNKNOWN = 0,
    NAZE32,
    NAZE32_REV5,
    NAZE32_SP
} nazeHardwareRevision_e;
extern uint8_t hardwareRevision;
void updateHardwareRevision(void);
void detectHardwareRevision(void);
void spiBusInit(void);
