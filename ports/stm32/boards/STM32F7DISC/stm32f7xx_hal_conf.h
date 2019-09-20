/* This file is part of the MicroPython project, http://micropython.org/
 * The MIT License (MIT)
 * Copyright (c) 2019 Damien P. George
 */
#ifndef MICROPY_INCLUDED_STM32F7XX_HAL_CONF_H
#define MICROPY_INCLUDED_STM32F7XX_HAL_CONF_H

#include "boards/stm32f7xx_hal_conf_base.h"

// Oscillator values in Hz
#define HSE_VALUE (25000000)
#define LSE_VALUE (32768)
#define EXTERNAL_CLOCK_VALUE (12288000)

// Oscillator timeouts in ms
#define HSE_STARTUP_TIMEOUT (5000)
#define LSE_STARTUP_TIMEOUT (5000)

#include "stm32f7xx_hal_dma2d.h"
#include "stm32f7xx_hal_ltdc.h"
#define HAL_DMA2D_MODULE_ENABLED
#define HAL_LTDC_MODULE_ENABLED

#endif // MICROPY_INCLUDED_STM32F7XX_HAL_CONF_H
