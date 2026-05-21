/*
 * lab16-1.c
 *
 * Joystick ADC example
 *
 * Replace the "<-YOUR CODE HERE->" comments with your code.
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

const uint8_t figures[20] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67,  // 0 - 9
                             0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x76, 0x30, 0x1E}; // A - J
uint8_t digit;
void set_joystick_read_axis(uint8_t axis)
{
    if (axis)
    {

        ADMUX &= 0xF0;
        ADMUX |= 1;
    }
    else
    {
        ADMUX &= 0xF0;
    }
}
void initialise_joystick(void)
{
    // SET UP THE JOYSTICK
    // `axis` will be 0 to read the X-axis of
    // the joystick, or 1 to read the Y-axis
    // set the ADC to use AVCC as the reference voltage
    // leave the ADC as right-adjust
    // no need to touch the MUX bits for now - they'll be updated below as we choose which axis to read from
    ADMUX = (1 << REFS0);
    // enable the ADC, and use a clock divisor to get between 50kHz and 200kHz
    ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (ADPS1) /* <YOUR CODE HERE> */;

    // if `axis` is 0, set the MUX[4:0] bits of ADMUX to read from ADC0 = pin A0 = X-axis
    set_joystick_read_axis(0);
    // start the ADC conversion
    ADCSRA |= (1 << ADSC) /* <YOUR CODE HERE> */;

    // if `axis` is 1, we just read the Y-axis, so allocate the ADC result to `y`
    // otherwise, we just read the X-axis, so allocate the result to `x`
    // toggle `axis`, so that the next loop will read the other joystick axis
}
