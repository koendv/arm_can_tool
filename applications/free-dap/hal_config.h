#ifndef FREEDAP_HAL_CONFIG_H
#define FREEDAP_HAL_CONFIG_H

#ifdef USE_SLOW_GPIO
#include "hal_rtthread.h"
#else
#include "hal_gpioregs.h"
#endif

#endif
