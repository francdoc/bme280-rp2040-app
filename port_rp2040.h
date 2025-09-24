#include "common/common.h"

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"

#include "bme280/bme280.h"

extern HAL_Sleep sleep_nano;
extern HAL_SPI hal_spi;
extern HAL_PinOut pin;

extern void bme280_run();