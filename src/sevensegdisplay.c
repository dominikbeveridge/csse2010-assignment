#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

/* Seven segment display segment values for 0 to 16 */
uint8_t seven_seg[16] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111, 119, 124, 57, 94, 121, 113};

void ss_render_number(int lr_control, uint8_t number, int decimal_point)
{
    number = number % 16;
    PORTC = seven_seg[number] | ((decimal_point & 1) << 7);

    PORTD = (PORTD & ~(1 << PD7)) | ((lr_control & 1) << PD7);
}

void ss_render_hyphen(int lr_control, int decimal_point)
{
    PORTC = 0b1000000 | ((decimal_point & 1) << 7);

    PORTD = (PORTD & ~(1 << PD7)) | ((lr_control & 1) << PD7);
}

void ss_render_blank(int lr_control)
{

    PORTC = 0;

    PORTD = (PORTD & ~(1 << PD7)) | ((lr_control & 1) << PD7);
}
