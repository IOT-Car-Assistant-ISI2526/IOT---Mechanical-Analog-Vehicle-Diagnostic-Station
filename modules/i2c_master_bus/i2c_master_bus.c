#include "i2c_master_bus.h"

const char *TAG = "I2C_MASTER_BUS";

i2c_master_bus_handle_t i2c_initialize_master(i2c_port_t port, int sda, int scl)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle = NULL;
    printf("Attempting I2C Init on SDA=%d, SCL=%d\n", sda, scl);
    fflush(stdout);

    esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &bus_handle);

    if (err != ESP_OK)
    {
        printf("CRITICAL ERROR: I2C Init failed: %s\n", esp_err_to_name(err));
        fflush(stdout);
        abort();
    }

    printf("I2C Bus Init Success. Handle: %p\n", bus_handle);
    fflush(stdout);

    return bus_handle;
}