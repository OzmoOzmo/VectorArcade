#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "soc/rtc.h"
#include "soc/sens_reg.h"
#include "soc/soc.h"

extern uint32_t fastDacCleanReg1;
extern uint32_t fastDacCleanReg2;
extern portMUX_TYPE fastDacSpinlock;


static inline void DACPrepare(bool useMutex)
{
  if (useMutex)
  {
    portENTER_CRITICAL(&fastDacSpinlock);
  }
  CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);
  CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
  CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);
  fastDacCleanReg1 = READ_PERI_REG(RTC_IO_PAD_DAC1_REG) & ~((RTC_IO_PDAC1_DAC) << RTC_IO_PDAC1_DAC_S);
  fastDacCleanReg2 = READ_PERI_REG(RTC_IO_PAD_DAC2_REG) & ~((RTC_IO_PDAC2_DAC) << RTC_IO_PDAC2_DAC_S);
}

static inline void DAC1Write(int value)
{
  WRITE_PERI_REG(RTC_IO_PAD_DAC1_REG,
                 fastDacCleanReg1 | ((value & RTC_IO_PDAC1_DAC) << RTC_IO_PDAC1_DAC_S));
  //delayMicroseconds(10);
}

static inline void DAC2Write(int value)
{
  WRITE_PERI_REG(RTC_IO_PAD_DAC2_REG,
                 fastDacCleanReg2 | (((255-value) & RTC_IO_PDAC2_DAC) << RTC_IO_PDAC2_DAC_S));
  //delayMicroseconds(10);
}

static inline void DACUnprepare(bool useMutex)
{
  if (useMutex)
  {
    portEXIT_CRITICAL(&fastDacSpinlock);
  }
}