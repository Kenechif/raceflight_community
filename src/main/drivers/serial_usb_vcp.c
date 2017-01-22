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
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "platform.h"
#include "build_config.h"
#include "usb_core.h"
#if defined(STM32F411xE) || defined(STM32F40_41xxx) || defined(STM32F446xx)
#include "usbd_cdc_vcp.h"
#else
#include "usb_init.h"
#include "hw_config.h"
#endif
#include "common/utils.h"
#include "drivers/system.h"
#include "serial.h"
#include "serial_usb_vcp.h"
#define USB_TIMEOUT 50
static vcpPort_t vcpPort;
static void usbVcpSetBaudRate(serialPort_t *instance, uint32_t baudRate)
{
    UNUSED(instance);
    UNUSED(baudRate);
}
static void usbVcpSetMode(serialPort_t *instance, portMode_t mode)
{
    UNUSED(instance);
    UNUSED(mode);
}
static bool isUsbVcpTransmitBufferEmpty(serialPort_t *instance)
{
    UNUSED(instance);
    return true;
}
static uint32_t usbVcpAvailable(serialPort_t *instance)
{
    UNUSED(instance);
    return receiveLength;
}
static uint8_t usbVcpRead(serialPort_t *instance)
{
    UNUSED(instance);
    uint8_t buf[1];
    uint32_t rxed = 0;
    while (rxed < 1) {
        rxed += CDC_Receive_DATA((uint8_t*)buf + rxed, 1 - rxed);
    }
    return buf[0];
}
static bool usbVcpFlush(vcpPort_t *port)
{
    uint8_t count = port->txAt;
    port->txAt = 0;
    if (count == 0) {
        return true;
    }
    if (!usbIsConnected() || !usbIsConfigured()) {
        return false;
    }
    uint32_t txed;
 uint32_t start = millis();
    do {
        txed = CDC_Send_DATA(port->txBuf, count);
 } while (txed != count && (millis() - start < USB_TIMEOUT));
    return txed == count;
}
static void usbVcpWrite(serialPort_t *instance, uint8_t c)
{
    vcpPort_t *port = container_of(instance, vcpPort_t, port);
    port->txBuf[port->txAt++] = c;
    if (!port->buffering || port->txAt >= ARRAYLEN(port->txBuf)) {
        usbVcpFlush(port);
    }
}
static void usbVcpBeginWrite(serialPort_t *instance)
{
    vcpPort_t *port = container_of(instance, vcpPort_t, port);
    port->buffering = true;
}
static void usbVcpEndWrite(serialPort_t *instance)
{
    vcpPort_t *port = container_of(instance, vcpPort_t, port);
 port->buffering = false;
    usbVcpFlush(port);
}
uint8_t usbTxBytesFree() {
    return 255;
}
static const struct serialPortVTable usbVTable[] = {
    {
    .serialWrite = usbVcpWrite,
    .serialTotalRxWaiting = usbVcpAvailable,
    .serialRead = usbVcpRead,
    .serialSetBaudRate = usbVcpSetBaudRate,
    .isSerialTransmitBufferEmpty = isUsbVcpTransmitBufferEmpty,
    .setMode = usbVcpSetMode,
    .beginWrite = usbVcpBeginWrite,
    .endWrite = usbVcpEndWrite,
    }
};
serialPort_t *usbVcpOpen(void)
{
    vcpPort_t *s;
#if defined(STM32F411xE) || defined(STM32F40_41xxx) || defined(STM32F446xx)
 USBD_Init(&USB_OTG_dev,
             USB_OTG_FS_CORE_ID,
             &USR_desc,
             &USBD_CDC_cb,
             &USR_cb);
#else
    Set_System();
    Set_USBClock();
    USB_Interrupts_Config();
    USB_Init();
#endif
    s = &vcpPort;
    s->port.vTable = usbVTable;
    return (serialPort_t *)s;
}
