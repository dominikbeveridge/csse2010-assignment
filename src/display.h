/*
 * display.h
 *
 * Author: Ryan Wang
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

/*
 * display a start screen
 */
void start_splash_display(void);

/*
 * draws a small char glyph on the LED matrix starting at x_position (columnn number)
 */
void draw_small_char(char character, uint8_t x_position, uint8_t colour);
void shift_display_left(int amount);
void draw_character_history(char character_history[50], uint8_t colour_history[50], int character_index, int font_large);
void draw_char(char character, uint8_t x_position, uint8_t colour, int font_large);
#endif
