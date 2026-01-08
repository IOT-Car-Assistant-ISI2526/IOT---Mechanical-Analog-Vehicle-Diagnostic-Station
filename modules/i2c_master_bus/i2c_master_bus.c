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
    fflush(stdout); // FORCE PRINT

    esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &bus_handle);

    // 2. CHECK THE ERROR
    if (err != ESP_OK)
    {
        printf("CRITICAL ERROR: I2C Init failed: %s\n", esp_err_to_name(err));
        fflush(stdout); // FORCE PRINT
        abort();        // Stop execution here so you can read the error
    }

    printf("I2C Bus Init Success. Handle: %p\n", bus_handle);
    fflush(stdout);

    return bus_handle;
}

// esp_err_t init_i2c_bus(i2c_port_t port, int sda, int scl, uint32_t freq)
// {
//     i2c_config_t conf = {
//         .mode = I2C_MODE_MASTER,
//         .sda_io_num = sda,
//         .scl_io_num = scl,
//         .sda_pullup_en = GPIO_PULLUP_ENABLE,
//         .scl_pullup_en = GPIO_PULLUP_ENABLE,
//         .master.clk_speed = freq,
//     };

//     ESP_ERROR_CHECK(i2c_param_config(port, &conf));
//     return i2c_driver_install(port, conf.mode, 0, 0, 0);
// }