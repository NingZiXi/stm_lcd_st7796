/** @file stm_lcd_st7796.c @brief 面板命令与实例生命周期。 */
#include "stm_lcd_st7796.h"
#include <limits.h>
#include <stdlib.h>

typedef struct { uint8_t cmd, size; uint16_t delay_ms; uint8_t data[14]; } init_command_t;
/* Board vendor SPI example, experiment 50; values are module-specific defaults. */
static const init_command_t init_commands[] = {
    {0x11, 0, 120, {0}},
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
    {0x21, 0, 0, {0}},
    {0x29, 0, 0, {0}},
};

struct lcd_st7796_context { lcd_st7796_config_t config; uint8_t initialized; };
stm_err_t lcd_st7796_create(const lcd_st7796_config_t *config, lcd_st7796_handle_t *out)
{
    if (!config || !out) return STM_ERR_INVALID_ARG;
    if (*out) return STM_ERR_INVALID_STATE;
    if (!config->tx_param || !config->delay_ms || !config->tx_color || !config->width || !config->height ||
        (uint32_t)config->width + config->x_gap > 65536u ||
        (uint32_t)config->height + config->y_gap > 65536u) return STM_ERR_INVALID_CONFIG;
    lcd_st7796_handle_t panel = calloc(1, sizeof(*panel));
    if (!panel) return STM_ERR_NO_MEM;
    panel->config = *config;
    *out = panel;
    return STM_OK;
}
stm_err_t lcd_st7796_delete(lcd_st7796_handle_t *handle)
{
    if (!handle) return STM_ERR_INVALID_ARG;
    free(*handle);
    *handle = NULL;
    return STM_OK;
}
stm_err_t lcd_st7796_reset(lcd_st7796_handle_t panel)
{
    if (!panel) return STM_ERR_INVALID_ARG;
    panel->initialized = 0;
    if (!panel->config.reset) return STM_OK;
    stm_err_t err;
    err = panel->config.reset(panel->config.io, 1);
    if (err != STM_OK) return err;
    panel->config.delay_ms(panel->config.io, 10);
    err = panel->config.reset(panel->config.io, 0);
    if (err != STM_OK) return err;
    panel->config.delay_ms(panel->config.io, 50);
    err = panel->config.reset(panel->config.io, 1);
    if (err != STM_OK) return err;
    panel->config.delay_ms(panel->config.io, 200);
    return STM_OK;
}
stm_err_t lcd_st7796_init(lcd_st7796_handle_t panel)
{
    if (!panel) return STM_ERR_INVALID_ARG;
    panel->initialized = 0;
    stm_err_t err;
    for (size_t i = 0; i < sizeof(init_commands) / sizeof(init_commands[0]); ++i) {
        const init_command_t *op = &init_commands[i];
        err = panel->config.tx_param(panel->config.io, op->cmd, op->data, op->size);
        if (err != STM_OK) return err;
        if (op->delay_ms) panel->config.delay_ms(panel->config.io, op->delay_ms);
    }
    panel->initialized = 1;
    return STM_OK;
}
stm_err_t lcd_st7796_display_on_off(lcd_st7796_handle_t panel, int on)
{
    if (!panel || (on != 0 && on != 1)) return STM_ERR_INVALID_ARG;
    if (!panel->initialized) return STM_ERR_INVALID_STATE;
    return panel->config.tx_param(panel->config.io, on ? 0x29 : 0x28, NULL, 0);
}
stm_err_t lcd_st7796_draw_bitmap(lcd_st7796_handle_t panel, uint16_t x1, uint16_t y1,
    uint16_t x2, uint16_t y2, const void *pixels)
{
    if (!panel || !pixels || x1 >= x2 || y1 >= y2) return STM_ERR_INVALID_ARG;
    if (x2 > panel->config.width || y2 > panel->config.height) return STM_ERR_OUT_OF_RANGE;
    size_t width = x2 - x1, height = y2 - y1;
    if (width > SIZE_MAX / height / 2u) return STM_ERR_OUT_OF_RANGE;
    if (!panel->initialized) return STM_ERR_INVALID_STATE;
    uint32_t xa = (uint32_t)x1 + panel->config.x_gap, xb = (uint32_t)x2 - 1u + panel->config.x_gap;
    uint32_t ya = (uint32_t)y1 + panel->config.y_gap, yb = (uint32_t)y2 - 1u + panel->config.y_gap;
    uint8_t x[4] = {(uint8_t)(xa >> 8), (uint8_t)xa, (uint8_t)(xb >> 8), (uint8_t)xb};
    uint8_t y[4] = {(uint8_t)(ya >> 8), (uint8_t)ya, (uint8_t)(yb >> 8), (uint8_t)yb};
    stm_err_t err = panel->config.tx_param(panel->config.io, 0x2A, x, sizeof x);
    if (err != STM_OK) return err;
    err = panel->config.tx_param(panel->config.io, 0x2B, y, sizeof y);
    if (err != STM_OK) return err;
    return panel->config.tx_color(panel->config.io, 0x2C, pixels, width * height * 2u);
}
