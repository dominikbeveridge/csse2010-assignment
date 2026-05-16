/*
 * morse.c
 *
 * Main file
 *
 * Authors: Peter Sutton, Bradley Stone, Ryan Wang
 * Modified by <YOUR NAME HERE>, <YOUR STUDENT ID HERE>
 */

/* Definitions */
#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <ctype.h>

/* Internal Library Includes */
#include "serialio.h"
#include "terminalio.h"
#include "ledmatrix.h"
#include "display.h"
#include "encoding.h"
#include "timer.h"
#include "sevensegdisplay.h"

/* Internal Function Declarations */
void initialise_hardware(void);
void start_morse(void);
void start_splash_screen(void);
void handle_inputs(void);

int prev_b0 = 1;
int prev_b1 = 1;
int prev_b2 = 1;
uint8_t led_io_state = 0b00000000;
uint8_t current_morse_state = 1;
uint8_t matrix_animation_frame = 0;
uint16_t led_io_animation_frame = 0;
uint64_t led_io_animation = 0;

uint8_t marks_inputted = 0;
uint8_t characters_inputted = 0;
uint8_t ssd_mux = 0;

int cursor_x = 1;
int cursor_y = 1;
int consecutive_submits = 0;

int synchonous_mode = 1;
uint32_t b0_hold_time_ms = 0;
uint32_t b0_release_time_ms = 0;

int main(void)
{
    initialise_hardware();
    start_splash_screen();
    start_morse();
}

void initialise_hardware(void)
{
    spi_setup_master(128); // init LED matrix
    // Setup serial port for 19200 baud communication

    init_serial_stdio(19200);
    printf("\033[1;39m");
    sei(); // enable global interrupts
    // enable L2 to L7 on A2 to A7
    DDRA |= 0b111111 << 2;
    // Enable L0 to L1 on D2 to D3
    DDRD |= 0b11 << 2;
    // SSD Display
    DDRC = 0xFF;
    // SSD LeftRight control
    DDRD |= (1 << PD7);
}
void start_splash_screen(void)
{
    // draw sigil on LED matrix
    start_splash_display();
    move_terminal_cursor(10, 6);
    printf("CSSE%d AVR Project", 2010); // change if masters student
    move_terminal_cursor(10, 8);
    printf("\"Morse Code Emulator\"");
    move_terminal_cursor(10, 10);
    printf("%d, Semester %s", 2026, "One");
    move_terminal_cursor(10, 12);
    // "%ld" is "long decimal", since a student number is bigger than 2**16
    printf("By %s (%ld)", "Student Name", 49642856);

    // Wait until a button is pressed
    while (!(PINB & 0x07))
    {
        ; // do nothing til button press
    }
    ledmatrix_clear();
}
void finish_animations()
{

    if (matrix_animation_frame != 0)
    {
        shift_display_left(matrix_animation_frame);
        matrix_animation_frame = 0;
    }

    if (led_io_animation_frame != 0)
    {
        while (led_io_animation_frame > 0)
        {
            uint8_t next_led = led_io_animation >> (led_io_animation_frame - 1);
            led_io_state <<= 1;
            led_io_state |= next_led;
            led_io_animation_frame--;
        }
    }
}

void start_morse(void)
{
    // Clear the serial terminal
    clear_terminal();
    move_terminal_cursor(cursor_x, cursor_y);
    initialise_100ms_timer();

    while (1)
    {
        // Handle any button or key inputs
        handle_inputs();
        if (serial_input_available())
        {

            int input = fgetc(stdin);
            if (isalnum(input))
            {
                finish_animations();
                current_morse_state = 1;
                marks_inputted = 0;
                char character = toupper(input);
                uint8_t morse = char_to_morse(character);
                int i;
                int first_bit_found = 0;

                led_io_animation = 0;
                if (consecutive_submits == 0)
                {

                    led_io_animation_frame = 1;
                }
                for (i = 7; i >= 0; i--)
                {
                    // loop through all bits of the morse code until we find the start
                    int bit = (morse & (1 << i)) >> i;

                    if (!first_bit_found && bit)
                    {
                        first_bit_found = 1;
                        continue;
                    }

                    if (first_bit_found)
                    {
                        marks_inputted++;
                        if (bit)
                        {
                            // queue dash to be animated
                            led_io_animation_frame += 3;
                            led_io_animation <<= 3;
                            led_io_animation |= 0b111;
                        }
                        else
                        {
                            // queue dot to be animated
                            led_io_animation_frame += 1;
                            led_io_animation <<= 1;
                            led_io_animation |= 0b1;
                        }
                        if (i == 0)
                        {
                            // add character submit
                            led_io_animation_frame += 3;
                            led_io_animation <<= 3;
                        }
                        else
                        {
                            // add mark submit
                            led_io_animation_frame += 1;
                            led_io_animation <<= 1;
                        }
                    }
                }
                printf("\033[1;33m%c", character);
                cursor_x++;
                draw_small_char(character, MATRIX_NUM_COLUMNS, COLOUR_YELLOW);
                characters_inputted++;
                matrix_animation_frame = 4;
                consecutive_submits = 1;
            }
        }

        PORTA = led_io_state;
        PORTD &= ~(0b11 << 2);
        PORTD |= ((led_io_state) & (0b11)) << 2;
        ssd_mux++;
        if (ssd_mux >= 0 && ssd_mux < 100)
        {
            if (marks_inputted > 0 && marks_inputted < 10)
            {
                ss_render_number(0, marks_inputted, 0);
            }
            else if (marks_inputted >= 10)
            {
                ss_render_hyphen(0, 0);
            }
            else if (marks_inputted == 0)
            {
                ss_render_blank(0);
            }
        }
        else
        {
            uint8_t decimal_point = marks_inputted > 0;
            ss_render_number(1, characters_inputted, decimal_point);
        }
        if (ssd_mux > 200)
        {
            ssd_mux = 0;
        }
    }
    // should never reach
}

void handle_dot()
{

    marks_inputted++;

    if (consecutive_submits > 0)
    {

        led_io_animation_frame = 1;
        led_io_animation = 0b1;
    }
    else
    {
        led_io_animation_frame = 2;
        led_io_animation = 0b01;
    }

    current_morse_state <<= 1;
    consecutive_submits = 0;

    char character = morse_to_char(current_morse_state);

    move_terminal_cursor(cursor_x, cursor_y);
    printf("\033[1;31m%c", character);
    draw_small_char(character, MATRIX_NUM_COLUMNS, COLOUR_RED);
}

void handle_dash()
{
    marks_inputted++;
    if (consecutive_submits > 0)
    {

        led_io_animation_frame = 3;
        led_io_animation = 0b111;
    }
    else
    {
        led_io_animation_frame = 4;
        led_io_animation = 0b0111;
    }

    current_morse_state <<= 1;
    current_morse_state |= 1;
    consecutive_submits = 0;

    char character = morse_to_char(current_morse_state);

    move_terminal_cursor(cursor_x, cursor_y);
    printf("\033[1;31m%c", character);
    draw_small_char(character, MATRIX_NUM_COLUMNS, COLOUR_RED);
}

void handle_submit()
{
    marks_inputted = 0;
    consecutive_submits++;
    if (consecutive_submits == 1)
    {
        // Submit character
        led_io_animation_frame = 3;
        led_io_animation = 0b000;
        char character = morse_to_char(current_morse_state);
        move_terminal_cursor(cursor_x, cursor_y);
        printf("\033[1;32m%c", character);
        cursor_x++;
        draw_small_char(character, MATRIX_NUM_COLUMNS, COLOUR_GREEN);
        characters_inputted++;
        matrix_animation_frame = 4;
    }
    else if (consecutive_submits == 2)
    {
        // Submit word
        printf(" ");
        cursor_x++;
        led_io_animation_frame = 2;
        led_io_animation = 0b00;
        matrix_animation_frame = 4;
    }

    current_morse_state = 1;
}

void handle_asynchronous_inputs(void)
{

    int b0_pressed = PINB & (1 << PB0);
    int b1_pressed = PINB & (1 << PB1);
    int b2_pressed = PINB & (1 << PB2);

    int b0_rising_edge = b0_pressed && !prev_b0;
    int b1_rising_edge = b1_pressed && !prev_b1;
    int b2_rising_edge = b2_pressed && !prev_b2;

    if ((b0_rising_edge || b1_rising_edge || b2_rising_edge)) // button pressed
    {
        finish_animations();
    }
    if (b0_rising_edge)
    {
        handle_dot();
    }

    if (b1_rising_edge)
    {
        handle_dash();
    }

    if (b2_rising_edge)
    {
        handle_submit();
    }

    prev_b0 = b0_pressed;
    prev_b1 = b1_pressed;
    prev_b2 = b2_pressed;
}

void handle_synchronous_inputs()
{
    int b0_pressed = PINB & (1 << PB0);

    if (!b0_pressed && prev_b0) // b0 released (falling edge)
    {
        finish_animations();
        if (b0_hold_time_ms <= 200)
        {
            handle_dot();
        }
        else
        {
            handle_dash();
        }
    }

    if (b0_release_time_ms > 1000 && consecutive_submits == 0)
    {
        finish_animations();
        handle_submit();
    }
    if (b0_release_time_ms > 2000 && consecutive_submits == 1)
    {
        finish_animations();
        handle_submit();
    }

    prev_b0 = b0_pressed;
}

void handle_inputs(void)
{
    /* ******** START HERE ********

    Read the button. Enter a mark if there is a rising edge on b0.
    A way to do this is to check if the previous b0 state is 0,
    and the current b0 state is a 1.
    (You will need to implement a method of tracking the previous b0 state.)
    Ensure that when you press a button to exit the splash screen,
    that this button press doesn't immediately trigger an input here.

    --. --- --- -.. / .-.. ..- -.-. -.-
    */
    synchonous_mode = (PIND >> PD6) & 1;
    if (!synchonous_mode)
    {
        handle_asynchronous_inputs();
    }
    else
    {

        handle_synchronous_inputs();
    }
}

// Timer 1 is initalised to 100ms frequency
ISR(TIMER1_COMPA_vect)
{
    if (matrix_animation_frame > 0)
    {
        shift_display_left(1);
        matrix_animation_frame--;
    }

    if (led_io_animation_frame > 0)
    {
        uint8_t next_led = led_io_animation >> (led_io_animation_frame - 1);
        led_io_state <<= 1;
        led_io_state |= next_led;
        led_io_animation_frame--;
    }

    int b0_released = !(PINB & (1 << PB0));
    if (b0_released)
    {
        b0_hold_time_ms = 0;
        b0_release_time_ms += 100;
    }
    else
    {
        b0_release_time_ms = 0;
        b0_hold_time_ms += 100;
    }
}