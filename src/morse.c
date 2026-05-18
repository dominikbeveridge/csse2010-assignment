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
#include <string.h>

/* Internal Library Includes */
#include "serialio.h"
#include "terminalio.h"
#include "ledmatrix.h"
#include "display.h"
#include "encoding.h"
#include "timer.h"
#include "sevensegdisplay.h"
#include "buzzer.h"

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

uint8_t submit_animation_frames[2] = {4, 7};

// stores character history. History is stored from left to right (left is oldest)
char character_history[50];
uint8_t colour_history[50];
int character_index = 0;
void save_history(char arr[50], char character)
{
    if (character_index < 50)
    {
        arr[character_index] = character;
    }
    else
    {
        character_index = 49;
        memmove(arr, arr + 1, 50 - 1);
        arr[character_index] = character;
    }
}
int previous_large = 0;
int is_font_large = 0;

int buzzer_animation_frame = 50;
float buzzer_duty_cycle_animation[50];
uint16_t buzzer_frequency_animation[50];
float BASE_FREQUENCY = 200.0;

uint8_t marks_inputted = 0;
uint8_t characters_inputted = 0;
uint8_t ssd_mux = 0;

int cursor_x = 1;
int cursor_y = 1;
int consecutive_submits = 0;

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
    initialise_piezo();
    play_piezo(200, 100); // silence it
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
    for (int i = 0; i < 50; i++)
    {

        buzzer_duty_cycle_animation[i] = 100;
        buzzer_frequency_animation[i] = 0;
    }
    buzzer_animation_frame = 0;
}

void handle_serial_input()
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
        buzzer_animation_frame = 1;
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

                    for (int j = 0; j < 3; j++)
                    {

                        buzzer_duty_cycle_animation[buzzer_animation_frame] = 50;
                        buzzer_frequency_animation[buzzer_animation_frame] = 600;
                        buzzer_animation_frame++;
                    }
                }
                else
                {
                    // queue dot to be animated
                    led_io_animation_frame += 1;
                    led_io_animation <<= 1;
                    led_io_animation |= 0b1;

                    buzzer_duty_cycle_animation[buzzer_animation_frame] = 50;
                    buzzer_frequency_animation[buzzer_animation_frame] = 400;
                    buzzer_animation_frame++;
                }
                if (i == 0)
                {
                    // add character submit
                    led_io_animation_frame += 3;
                    led_io_animation <<= 3;
                    buzzer_duty_cycle_animation[buzzer_animation_frame - 1] = 10;
                    if (bit)
                    {
                        buzzer_duty_cycle_animation[buzzer_animation_frame - 2] = 10;
                        buzzer_duty_cycle_animation[buzzer_animation_frame - 3] = 10;
                    }
                }
                else
                {
                    // add mark submit
                    led_io_animation_frame += 1;
                    led_io_animation <<= 1;

                    buzzer_duty_cycle_animation[buzzer_animation_frame] = 100;
                    buzzer_frequency_animation[buzzer_animation_frame] = 500;
                    buzzer_animation_frame++;
                }
            }
        }

        printf("\033[1;33m%c", character);
        cursor_x++;
        draw_char(character, MATRIX_NUM_COLUMNS, COLOUR_YELLOW, is_font_large);
        save_history(character_history, character);
        save_history(colour_history, COLOUR_YELLOW);
        character_index++;
        characters_inputted++;
        matrix_animation_frame = submit_animation_frames[is_font_large];
        consecutive_submits = 1;
        buzzer_animation_frame = 0;
    }
}
void render_ssd()
{
    // Render SSD
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
        // Handle serial input
        if (serial_input_available())
        {
            handle_serial_input();
        }

        // render LED state
        PORTA = led_io_state;
        PORTD &= ~(0b11 << 2);
        PORTD |= ((led_io_state) & (0b11)) << 2;

        render_ssd();
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

    save_history(character_history, character);
    save_history(colour_history, COLOUR_RED);

    move_terminal_cursor(cursor_x, cursor_y);
    printf("\033[1;31m%c", character);
    draw_char(character, MATRIX_NUM_COLUMNS, COLOUR_RED, is_font_large);
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
    save_history(character_history, character);
    save_history(colour_history, COLOUR_RED);
    move_terminal_cursor(cursor_x, cursor_y);
    printf("\033[1;31m%c", character);
    draw_char(character, MATRIX_NUM_COLUMNS, COLOUR_RED, is_font_large);
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

        save_history(character_history, character);
        save_history(colour_history, COLOUR_GREEN);
        character_index++;
        move_terminal_cursor(cursor_x, cursor_y);
        printf("\033[1;32m%c", character);
        cursor_x++;
        draw_char(character, MATRIX_NUM_COLUMNS, COLOUR_GREEN, is_font_large);
        characters_inputted++;
        matrix_animation_frame = submit_animation_frames[is_font_large];
    }
    else if (consecutive_submits == 2)
    {
        // Submit word
        printf(" ");
        character_index++;
        cursor_x++;
        led_io_animation_frame = 2;
        led_io_animation = 0b00;
        matrix_animation_frame = submit_animation_frames[is_font_large];
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

void rerender(int font_large)
{
    finish_animations();
    draw_character_history(character_history, colour_history, character_index, font_large);
}

void handle_inputs(void)
{

    is_font_large = (PIND >> PD5) & 1;
    if (is_font_large && !previous_large)
    {
        rerender(is_font_large);
    }
    else if (!is_font_large && previous_large)
    {
        rerender(is_font_large);
    }
    previous_large = is_font_large;

    int synchonous_mode = (PIND >> PD4) & 1;
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
    // Animate queued matrix frames
    if (matrix_animation_frame > 0)
    {

        shift_display_left(1);

        matrix_animation_frame--;
    }
    // Animated queued LED frames
    if (led_io_animation_frame > 0)
    {
        uint8_t next_led = led_io_animation >> (led_io_animation_frame - 1);
        led_io_state <<= 1;
        led_io_state |= next_led;
        led_io_animation_frame--;
    }
    // Play queued buzzer tones
    if (buzzer_animation_frame < 50)
    {
        float duty_cycle = buzzer_duty_cycle_animation[buzzer_animation_frame];
        uint8_t frequency = buzzer_frequency_animation[buzzer_animation_frame];
        buzzer_animation_frame++;
        play_piezo(frequency, duty_cycle);
    }
    else
    {
        // stop buzzer
        play_piezo(200, 100);
    }

    // Count hold times
    int synchronous_mode = (PIND >> PD4) & 1;
    if (synchronous_mode)
    {

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
    else
    {
        b0_hold_time_ms = 0;
        b0_release_time_ms = 0;
    }
}