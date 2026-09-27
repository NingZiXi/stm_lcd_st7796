#include "stm_lcd_st7796.h"
#include <limits.h>
#include <string.h>

typedef struct { uint8_t cmd, size; uint16_t delay_ms; uint8_t data[14]; } init_command_t;
/* Board vendor SPI example, experiment 50; values are module-specific defaults. */
static const init_command_t init_commands[] = {
    {0x11, 0, 120, {}},
    {0x36, 1, 0, {0x48}},
    {0x3A, 1, 0, {0x55}},
    {0xF0, 1, 0, {0xC3}},
    {0xF0, 1, 0, {0x96}},
    {0xB4, 1, 0, {0x01}},
    {0xB6, 2, 0, {0x0A, 0xA2}},
    {0xB7, 1, 0, {0xC6}},
    {0xB9, 2, 0, {0x02, 0xE0}},
    {0xC0, 2, 0, {0x80, 0x16}},
    {0xC1, 1, 0, {0x19}},
    {0xC2, 1, 0, {0xA7}},
    {0xC5, 1, 0, {0x16}},
    {0xE8, 8, 0, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}},
    {0xE0, 14, 0, {0xF0, 0x07, 0x0D, 0x04, 0x05, 0x14, 0x36, 0x54, 0x4C, 0x38, 0x13, 0x14, 0x2E, 0x34}},
    {0xE1, 14, 0, {0xF0, 0x10, 0x14, 0x0E, 0x0C, 0x08, 0x35, 0x44, 0x4C, 0x26, 0x10, 0x12, 0x2C, 0x32}},
    {0xF0, 1, 0, {0x3C}},
    {0xF0, 1, 120, {0x69}},
    {0x21, 0, 0, {}},
    {0x29, 0, 0, {}},
};

int stm_lcd_st7796_new_panel(stm_lcd_st7796_t *panel, const stm_lcd_st7796_config_t *config)
{
    if (!panel || !config || !config->tx_param || !config->tx_color ||
        !config->width || !config->height ||
        (uint32_t)config->width + config->x_gap > 65536u ||
        (uint32_t)config->height + config->y_gap > 65536u) return -1;
    memset(panel, 0, sizeof(*panel));
    panel->config = *config;
    panel->created = 1;
    return 0;
}
int stm_lcd_st7796_reset(stm_lcd_st7796_t *panel)
{
    if (!panel || !panel->created) return -1;
    panel->initialized = 0;
    if (!panel->config.reset) return 0;
    if (!panel->config.delay_ms) return -1;
    panel->config.reset(panel->config.io, 1);
    panel->config.delay_ms(panel->config.io, 10);
    panel->config.reset(panel->config.io, 0);
    panel->config.delay_ms(panel->config.io, 50);
    panel->config.reset(panel->config.io, 1);
    panel->config.delay_ms(panel->config.io, 200);
    return 0;
}
int stm_lcd_st7796_init(stm_lcd_st7796_t *panel)
{
    size_t i;
    if (!panel || !panel->created || !panel->config.delay_ms) return -1;
    panel->initialized = 0;
    for (i = 0; i < sizeof(init_commands)/sizeof(init_commands[0]); ++i) {
        const init_command_t *op = &init_commands[i];
        if (panel->config.tx_param(panel->config.io, op->cmd, op->data, op->size)) return -2;
        if (op->delay_ms) panel->config.delay_ms(panel->config.io, op->delay_ms);
    }
    panel->initialized = 1;
    return 0;
}
int stm_lcd_st7796_draw_bitmap(stm_lcd_st7796_t *panel, uint16_t x1, uint16_t y1,
                           uint16_t x2, uint16_t y2, const void *pixels)
{
    uint8_t x[4], y[4];
    uint32_t xa, xb, ya, yb;
    size_t width, height;
    if (!panel || !panel->initialized || !pixels || x1 >= x2 || y1 >= y2 ||
        x2 > panel->config.width || y2 > panel->config.height) return -1;
    width = (size_t)(x2 - x1); height = (size_t)(y2 - y1);
    if (width > SIZE_MAX / height / 2u) return -1;
    xa = (uint32_t)x1 + panel->config.x_gap; xb = (uint32_t)x2 - 1u + panel->config.x_gap;
    ya = (uint32_t)y1 + panel->config.y_gap; yb = (uint32_t)y2 - 1u + panel->config.y_gap;
    x[0] = (uint8_t)(xa >> 8); x[1] = (uint8_t)xa;
    x[2] = (uint8_t)(xb >> 8); x[3] = (uint8_t)xb;
    y[0] = (uint8_t)(ya >> 8); y[1] = (uint8_t)ya;
    y[2] = (uint8_t)(yb >> 8); y[3] = (uint8_t)yb;
    if (panel->config.tx_param(panel->config.io, 0x2A, x, sizeof x) ||
        panel->config.tx_param(panel->config.io, 0x2B, y, sizeof y) ||
        panel->config.tx_color(panel->config.io, 0x2C, pixels, width * height * 2u)) return -2;
    return 0;
}
int stm_lcd_st7796_display_on_off(stm_lcd_st7796_t *panel, int on)
{
    if (!panel || !panel->initialized) return -1;
    return panel->config.tx_param(panel->config.io, on ? 0x29 : 0x28, NULL, 0) ? -2 : 0;
}
