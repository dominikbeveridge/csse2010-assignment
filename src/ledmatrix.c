/*
 * ledmatrix.c
 *
 * Author: Peter Sutton
 *
 * See the LED matrix Reference for details of the SPI commands used.
 */

#include <avr/io.h>
#include "ledmatrix.h"
#include "spi.h"

#define CMD_UPDATE_COL 0x03
#define CMD_CLEAR_SCREEN 0x0F
#define CMD_SHIFT 0x04
#define LEFT_SHIFT 0b0010
#define CMD_UPDATE_ALL 0x00

void ledmatrix_update_column(uint8_t x, uint8_t pixels[MATRIX_NUM_ROWS])

{
    if (x >= MATRIX_NUM_COLUMNS)
    {
        // x value is too large - we ignore the request
        return;
    }
    (void)spi_send_byte(CMD_UPDATE_COL);
    (void)spi_send_byte(x & 0x0F); // column number
    for (uint8_t y = 0; y < MATRIX_NUM_ROWS; y++)
    {
        (void)spi_send_byte(pixels[y]);
    }
}

void ledmatrix_clear(void)
{
    (void)spi_send_byte(CMD_CLEAR_SCREEN);
}
void ledmatrix_shift_left(void)
{
    (void)spi_send_byte(CMD_SHIFT);
    (void)spi_send_byte(LEFT_SHIFT);
}
// sure would be useful to be able to use the other LED matrix commands...

void ledmatrix_update_display(uint8_t pixels[MATRIX_NUM_COLUMNS * MATRIX_NUM_ROWS])
{
    (void)spi_send_byte(CMD_UPDATE_ALL);
    (void)spi_send_byte(pixels);
}