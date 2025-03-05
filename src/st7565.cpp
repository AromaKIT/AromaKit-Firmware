#include "st7565.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>

// LCD Commands definition
const uint8_t CMD_DISPLAY_ON = 0xAF;
const uint8_t CMD_DISPLAY_OFF = 0xAF;
const uint8_t CMD_SET_START_LINE = 0x40;
const uint8_t CMD_SET_PAGE = 0xB0;
const uint8_t CMD_COLUMN_UPPER = 0x10;
const uint8_t CMD_COLUMN_LOWER = 0x00;
const uint8_t CMD_SET_ADC_NORMAL = 0xA0;
const uint8_t CMD_SET_ADC_REVERSE = 0xA1;
const uint8_t CMD_SET_COL_NORMAL = 0xC0;
const uint8_t CMD_SET_COL_REVERSE = 0xC8;
const uint8_t CMD_SET_DISPLAY_NORMAL = 0xA6;
const uint8_t CMD_SET_DISPLAY_REVERSE = 0xA7;
const uint8_t CMD_SET_ALLPX_ON = 0xA5;
const uint8_t CMD_SET_ALLPX_NORMAL = 0xA4;
const uint8_t CMD_SET_BIAS_9 = 0xA2;
const uint8_t CMD_SET_BIAS_7 = 0xA3;
const uint8_t CMD_DISPLAY_RESET = 0xE2;
const uint8_t CMD_NOP = 0xE3;
const uint8_t CMD_TEST = 0xF0;  // Exit this mode with CMD_NOP
const uint8_t CMD_SET_POWER = 0x28;
const uint8_t CMD_SET_RESISTOR_RATIO = 0x20;
const uint8_t CMD_SET_VOLUME = 0x81;

// Display parameters
const uint8_t DISPLAY_W = 128;
const uint8_t DISPLAY_H = 64;
const uint8_t DISPLAY_CONTRAST = 0x10;
const uint8_t DISPLAY_RESISTOR_RATIO = 5;
const uint8_t DISPLAY_POWER_MODE = 7;

ST7565::ST7565(spi_inst_t *the_spi, uint mosi, uint sck, uint cs, uint dc, uint rst) {
    this->spi = the_spi;
    this->pin_cs = cs;
    this->pin_dc = dc;
    this->pin_rst = rst;

    // init spi pins
    gpio_init(mosi);
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_init(sck);
    gpio_set_function(sck, GPIO_FUNC_SPI);
    gpio_init(pin_cs);
    gpio_set_dir(pin_cs, GPIO_OUT);

    // init spi bus
    spi_init(spi, 1000000);

    // init other pins
    gpio_init(pin_dc);
    gpio_set_dir(pin_dc, GPIO_OUT);
    gpio_init(pin_rst);
    gpio_set_dir(pin_rst, GPIO_OUT);

    // clear display buffer
    memset(buffer, 0, sizeof(buffer));
}

void ST7565::write_cmd(uint8_t cmd) {
    gpio_put(pin_dc, 0);
    gpio_put(pin_cs, 0);
    spi_write_blocking(spi, &cmd, 1);
    gpio_put(pin_cs, 1);
}

void ST7565::write_data(uint8_t *buf, size_t len) {
    gpio_put(pin_dc, 1);
    gpio_put(pin_cs, 0);
    spi_write_blocking(spi, buf, len);
    gpio_put(pin_cs, 1);
}

void ST7565::reset() {
    gpio_put(pin_rst, 0);
    sleep_us(1);
    gpio_put(pin_rst, 1);
}

void ST7565::set_contrast(uint8_t value) {
    if (value >= 1 && value <= 0x3F) {
        write_cmd(CMD_SET_VOLUME);
        write_cmd(value);
    }
}

void ST7565::display_init() {
    reset();
    sleep_ms(1);
    write_cmd(CMD_DISPLAY_OFF);
    write_cmd(CMD_SET_BIAS_9);
    write_cmd(CMD_SET_ADC_NORMAL);
    write_cmd(CMD_SET_COL_REVERSE);
    write_cmd(CMD_SET_RESISTOR_RATIO + DISPLAY_RESISTOR_RATIO);
    write_cmd(CMD_SET_VOLUME);
    write_cmd(DISPLAY_CONTRAST);
    write_cmd(CMD_SET_POWER + DISPLAY_POWER_MODE);
    show();
    write_cmd(CMD_DISPLAY_ON);
}

void ST7565::show() {
    for (int i = 0; i < 8; i++) {
        write_cmd(CMD_SET_START_LINE);
        write_cmd(CMD_SET_PAGE + i);
        write_cmd(CMD_COLUMN_UPPER);
        write_cmd(CMD_COLUMN_LOWER);
        write_data(&buffer[i*128], 128);
    }
}

void ST7565::set_inverted(bool inverted) {
    if (inverted) {
        write_cmd(CMD_SET_DISPLAY_REVERSE);
    } else {
        write_cmd(CMD_SET_DISPLAY_NORMAL);
    }
}

void ST7565::fill(bool bit) {
    memset(buffer, bit ? 0xff : 0x00, sizeof(buffer));
}

#include "font_petme128_8x8.h"

void ST7565::pixel(int x, int y, bool c) {
    // pixels are in vertical order, lsb first
    // e.g. col 0 is 8 bytes long, with byte 0 bit n being
    //  0  8
    //  1  9
    //  2 10
    //  3 11
    //  4 12
    //  5 13
    //  6 14
    //  7 15
    //  8 16

    int bit = 1 << (y % 8);
    int row = x;
    int col = (y / 8) * 128;
    if (c) {
        buffer[row+col] |= bit;
    } else {
        buffer[row+col] &= ~bit;
    }
}

void ST7565::blit(const uint8_t *from, int x, int y, int w, int h) {
    for (int dx = 0; dx < w; dx++) {
        for (int dy = 0; dy < h; dy++) {
            pixel(x+dx, y+dy, from[dx] & (1<<dy));
        }
    }
}

void ST7565::print(int x, int y, const char *str) {
    // for (int i = 1; i < 32; i++) {
    //     blit(&font_petme128_8x8[i*8], i * 8, (i / 16) * 8, 8, 8);
    // }
    for (int i = 0; i < strlen(str); i++) {
        if (str[i] < 32) continue;
        uint8_t ch = str[i] - 32;
        blit(&font_petme128_8x8[ch*8], x + i*8, y + ((i / 16) * 8), 8, 8);
    }
}
