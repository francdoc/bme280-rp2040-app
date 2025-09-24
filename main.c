#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"

#include "common/common.h"
#include "port_rp2040.h"

int main()
{
    stdio_init_all();
    bme280_run();
    return 0;
}
