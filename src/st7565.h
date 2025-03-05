#ifndef ST7565_H
#define ST7565_H

#include <stdint.h>
#include "hardware/spi.h"

class ST7565 {
public:
    ST7565(spi_inst_t *spi, uint mosi, uint sck, uint cs, uint dc, uint rst);

    void display_init();
    void reset();
    void show();
    void fill(bool bit);
    void set_contrast(uint8_t value);
    void set_inverted(bool inverted);

    void write_cmd(uint8_t cmd);
    void write_data(uint8_t *buf, size_t len);

private:
    spi_inst_t *spi;
    uint pin_cs;
    uint pin_dc;
    uint pin_rst;

    uint8_t buffer[1024];
};

#endif // ST7565_H
