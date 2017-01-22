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
#include "common/utils.h"
#include "io.h"
#include "io_impl.h"
#include "rcc.h"
#include "target.h"
struct ioPortDef_s {
 rccPeriphTag_t rcc;
};
#if defined(STM32F10X)
const struct ioPortDef_s ioPortDefs[] = {
 { RCC_APB2(IOPA) },
 { RCC_APB2(IOPB) },
 { RCC_APB2(IOPC) },
 { RCC_APB2(IOPD) },
 { RCC_APB2(IOPE) },
{
#if defined (STM32F10X_HD) || defined (STM32F10X_XL) || defined (STM32F10X_HD_VL)
 RCC_APB2(IOPF),
#else
 0,
#endif
},
{
#if defined (STM32F10X_HD) || defined (STM32F10X_XL) || defined (STM32F10X_HD_VL)
 RCC_APB2(IOPG),
#else
 0,
#endif
},
};
#elif defined(STM32F303xC)
const struct ioPortDef_s ioPortDefs[] = {
 { RCC_AHB(GPIOA) },
 { RCC_AHB(GPIOB) },
 { RCC_AHB(GPIOC) },
 { RCC_AHB(GPIOD) },
 { RCC_AHB(GPIOE) },
 { RCC_AHB(GPIOF) },
};
#elif defined(STM32F40_41xxx) || defined(STM32F411xE) || defined(STM32F446xx)
const struct ioPortDef_s ioPortDefs[] = {
 { RCC_AHB1(GPIOA) },
 { RCC_AHB1(GPIOB) },
 { RCC_AHB1(GPIOC) },
 { RCC_AHB1(GPIOD) },
 { RCC_AHB1(GPIOE) },
 { RCC_AHB1(GPIOF) },
};
# endif
ioRec_t* IO_Rec(IO_t io)
{
 return io;
}
GPIO_TypeDef* IO_GPIO(IO_t io)
{
 ioRec_t *ioRec = IO_Rec(io);
 return ioRec->gpio;
}
uint16_t IO_Pin(IO_t io)
{
 ioRec_t *ioRec = IO_Rec(io);
 return ioRec->pin;
}
int IO_GPIOPortIdx(IO_t io)
{
 if (!io)
  return -1;
 return (((size_t)IO_GPIO(io) - GPIOA_BASE) >> 10);
}
int IO_EXTI_PortSourceGPIO(IO_t io)
{
 return IO_GPIOPortIdx(io);
}
int IO_GPIO_PortSource(IO_t io)
{
 return IO_GPIOPortIdx(io);
}
int IO_GPIOPinIdx(IO_t io)
{
 if (!io)
  return -1;
 return 31 - __builtin_clz(IO_Pin(io));
}
int IO_EXTI_PinSource(IO_t io)
{
 return IO_GPIOPinIdx(io);
}
int IO_GPIO_PinSource(IO_t io)
{
 return IO_GPIOPinIdx(io);
}
uint32_t IO_EXTI_Line(IO_t io)
{
 if (!io)
  return 0;
#if defined(STM32F10X)
 return 1 << IO_GPIOPinIdx(io);
#elif defined(STM32F303xC)
 return IO_GPIOPinIdx(io);
#elif defined(STM32F40_41xxx) || defined(STM32F411xE) || defined(STM32F446xx)
 return 1 << IO_GPIOPinIdx(io);
#else
# error "Unknown target type"
#endif
}
bool IORead(IO_t io)
{
 if (!io)
  return false;
 return !! (IO_GPIO(io)->IDR & IO_Pin(io));
}
void IOWrite(IO_t io, bool hi)
{
 if (!io)
  return;
#if defined(STM32F40_41xxx) || defined(STM32F411xE) || defined(STM32F446xx)
    if (hi) {
        IO_GPIO(io)->BSRRL = IO_Pin(io);
    } else {
        IO_GPIO(io)->BSRRH = IO_Pin(io);
    }
#else
 IO_GPIO(io)->BSRR = IO_Pin(io) << (hi ? 0 : 16);
#endif
}
void IOHi(IO_t io)
{
 if (!io)
  return;
#if defined(STM32F40_41xxx) || defined(STM32F411xE) || defined(STM32F446xx)
    IO_GPIO(io)->BSRRL = IO_Pin(io);
#else
    IO_GPIO(io)->BSRR = IO_Pin(io);
#endif
    }
void IOLo(IO_t io)
{
 if (!io)
  return;
#if defined(STM32F40_41xxx) || defined(STM32F411xE) || defined(STM32F446xx)
    IO_GPIO(io)->BSRRH = IO_Pin(io);
#else
    IO_GPIO(io)->BRR = IO_Pin(io);
#endif
}
void IOToggle(IO_t io)
{
 if (!io)
  return;
 uint16_t mask = IO_Pin(io);
#if defined(STM32F40_41xxx) || defined(STM32F411xE) || defined(STM32F446xx)
 IO_GPIO(io)->ODR ^= mask;
#else
 if (IO_GPIO(io)->ODR & mask)
  mask <<= 16;
    IO_GPIO(io)->BSRR = mask;
#endif
}
void IOInit(IO_t io, resourceOwner_t owner, resourceType_t resources)
{
 ioRec_t *ioRec = IO_Rec(io);
 if (owner != OWNER_FREE)
  ioRec->owner = owner;
 ioRec->resourcesUsed |= resources;
}
void IORelease(IO_t io)
{
 ioRec_t *ioRec = IO_Rec(io);
 ioRec->owner = OWNER_FREE;
}
resourceOwner_t IOGetOwner(IO_t io)
{
 ioRec_t *ioRec = IO_Rec(io);
 return ioRec->owner;
}
resourceType_t IOGetResources(IO_t io)
{
 ioRec_t *ioRec = IO_Rec(io);
 return ioRec->resourcesUsed;
}
#if defined(STM32F10X)
void IOConfigGPIO(IO_t io, ioConfig_t cfg)
{
 if (!io)
  return;
 rccPeriphTag_t rcc = ioPortDefs[IO_GPIOPortIdx(io)].rcc;
 RCC_ClockCmd(rcc, ENABLE);
 GPIO_InitTypeDef init = {
  .GPIO_Pin = IO_Pin(io),
  .GPIO_Speed = cfg & 0x03,
  .GPIO_Mode = cfg & 0x7c,
 };
 GPIO_Init(IO_GPIO(io), &init);
}
#elif defined(STM32F303xC) || defined(STM32F40_41xxx) || defined(STM32F411xE) || defined(STM32F446xx)
void IOConfigGPIO(IO_t io, ioConfig_t cfg)
{
 if (!io)
  return;
 rccPeriphTag_t rcc = ioPortDefs[IO_GPIOPortIdx(io)].rcc;
 RCC_ClockCmd(rcc, ENABLE);
 GPIO_InitTypeDef init = {
  .GPIO_Pin = IO_Pin(io),
  .GPIO_Mode = (cfg >> 0) & 0x03,
  .GPIO_Speed = (cfg >> 2) & 0x03,
  .GPIO_OType = (cfg >> 4) & 0x01,
  .GPIO_PuPd = (cfg >> 5) & 0x03,
 };
 GPIO_Init(IO_GPIO(io), &init);
}
void IOConfigGPIOAF(IO_t io, ioConfig_t cfg, uint8_t af)
{
 if (!io)
  return;
 rccPeriphTag_t rcc = ioPortDefs[IO_GPIOPortIdx(io)].rcc;
 RCC_ClockCmd(rcc, ENABLE);
 GPIO_PinAFConfig(IO_GPIO(io), IO_GPIO_PinSource(io), af);
 GPIO_InitTypeDef init = {
  .GPIO_Pin = IO_Pin(io),
  .GPIO_Mode = (cfg >> 0) & 0x03,
  .GPIO_Speed = (cfg >> 2) & 0x03,
  .GPIO_OType = (cfg >> 4) & 0x01,
  .GPIO_PuPd = (cfg >> 5) & 0x03,
 };
 GPIO_Init(IO_GPIO(io), &init);
}
#endif
static const uint16_t ioDefUsedMask[DEFIO_PORT_USED_COUNT] = { DEFIO_PORT_USED_LIST };
static const uint8_t ioDefUsedOffset[DEFIO_PORT_USED_COUNT] = { DEFIO_PORT_OFFSET_LIST };
ioRec_t ioRecs[DEFIO_IO_USED_COUNT];
void IOInitGlobal(void) {
 ioRec_t *ioRec = ioRecs;
 for (unsigned port = 0; port < ARRAYLEN(ioDefUsedMask); port++)
  for (unsigned pin = 0; pin < sizeof(ioDefUsedMask[0]) * 8; pin++)
   if (ioDefUsedMask[port] & (1 << pin)) {
    ioRec->gpio = (GPIO_TypeDef *)(GPIOA_BASE + (port << 10));
    ioRec->pin = 1 << pin;
    ioRec++;
   }
}
IO_t IOGetByTag(ioTag_t tag)
{
 int portIdx = DEFIO_TAG_GPIOID(tag);
 int pinIdx = DEFIO_TAG_PIN(tag);
 if (portIdx >= DEFIO_PORT_USED_COUNT)
  return NULL;
 if (!(ioDefUsedMask[portIdx] & (1 << pinIdx)))
  return NULL;
 int offset = __builtin_popcount(((1 << pinIdx) - 1) & ioDefUsedMask[portIdx]);
 offset += ioDefUsedOffset[portIdx];
 return ioRecs + offset;
}
