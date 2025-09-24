#include "port_rp2040.h"

#define microseconds (1000)
#define milliseconds (1000 * microseconds)
#define seconds (1000LL * milliseconds)

#define READ_BIT 0x80
#define spi_default PICO_DEFAULT_SPI_INSTANCE
#define PICO_DEFAULT_SPI_RX_PIN 16
#define PICO_DEFAULT_SPI_CSN_PIN 17
#define PICO_DEFAULT_SPI_SCK_PIN 18
#define PICO_DEFAULT_SPI_TX_PIN 19
#define DELAY (250 * milliseconds)
#define CS_DELAY (15 * milliseconds)

BME280 spi_device;

HAL_Sleep sleep_nano;
HAL_SPI hal_spi;
HAL_PinOut pin;

/*
   GPIO 16 (pin 21) MISO/spi0_rx-> SDO/SDO on bme280 board
   GPIO 17 (pin 22) Chip select -> CSB/!CS on bme280 board
   GPIO 18 (pin 24) SCK/spi0_sclk -> SCL/SCK on bme280 board
   GPIO 19 (pin 25) MOSI/spi0_tx -> SDA/SDI on bme280 board
   3.3v (pin 36) -> VCC on bme280 board
   GND (pin 38)  -> GND on bme280 board

   https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf
*/

void set_cs_pin(bool state)
{
    gpio_put(spi_device.cs_pin, state); // port to rp2040
    printf("CS Pin set to: %d\n", state);
}

void sleep_nano_pico(int64 nanoseconds)
{
    if (nanoseconds < 0)
    {
        panic("negative time value");
    }
    sleep_us((uint64)(nanoseconds / 1000));
    printf("Sleeping for %lld nanoseconds\n", nanoseconds);
}

error spi_tx_wrapper(byte *write_data, isize wlen, byte *read_data, isize rlen)
{
    bool hasWriteData = write_data != NULL && wlen > 0;
    bool hasReadData = read_data != NULL && rlen > 0;

    printf("SPI TX Wrapper: 0x%x, hasWriteData: %d, hasReadData: %d\n", hasWriteData, hasReadData);

    if (hasWriteData)
    {
        int result = spi_write_blocking(spi_default, write_data, wlen);
        if (result < 0)
        {
            printf("SPI write error\n");
            return -1; // Indicate an error
        }
        printf("SPI write result: %d\n", result);
    }

    if (hasReadData)
    {
        int result = spi_read_blocking(spi_default, 0, read_data, rlen);
        if (result < 0)
        {
            printf("SPI read error\n");
            return -1; // Indicate an error
        }
        printf("SPI read result: %d\n", result);
    }

    return 0; // Success
}

void rp2040_setup()
{
    sleep_ms(2500); // to catch early prints when using ~$ minicom -b 115200 -o -D /dev/ttyACM0 in command shell terminal

    sleep_nano = &sleep_nano_pico;
    hal_spi.tx = &spi_tx_wrapper;
    pin = &set_cs_pin;

    spi_device = new_BME280(pin, READ_BIT, PICO_DEFAULT_SPI_CSN_PIN, 15, true, hal_spi, sleep_nano);

    printf("Hello, bme280! Reading raw data from registers via SPI...\n");

    // This example will use SPI0 at 0.5MHz.
    spi_init(spi_default, 500 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    bi_decl(bi_3pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI));

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
    // Make the CS pin available to picotool
    bi_decl(bi_1pin_with_name(PICO_DEFAULT_SPI_CSN_PIN, "SPI CS"));

    // See if SPI is working - interrograte the device for its ID number, should be 0x60
    uint8 id;
    read_registers(&spi_device, 0xD0, &id, 1);
    printf("Chip ID is 0x%x\n", id);

    read_compensation_parameters(&spi_device);

    write_register(&spi_device, 0xF2, 0x1);  // Humidity oversampling register - going for x1
    write_register(&spi_device, 0xF4, 0x27); // Set rest of oversampling modes and run mode to normal
}

void bme280_run()
{
    rp2040_setup();

    int32 humidity, pressure, temperature;

    while (1)
    {
        bme280_read_raw(&spi_device, &humidity, &pressure, &temperature);

        // These are the raw numbers from the chip, so we need to run through the
        // compensations to get human understandable numbers
        pressure = compensate_pressure(&spi_device, pressure);
        temperature = compensate_temp(&spi_device, temperature);
        humidity = compensate_humidity(&spi_device, humidity);

        printf("Humidity = %.2f%%\n", humidity / 1024.0);
        printf("Pressure = %dPa\n", pressure);
        printf("Temp. = %.2fC\n", temperature / 100.0);

        sleep_nano(250 * milliseconds);
    }
}