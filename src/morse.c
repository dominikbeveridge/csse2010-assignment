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
#include "joystick.h"

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

float DEADZONE = 100;
uint8_t submit_animation_frames[2] = {4, 7};

// stores character history. History is stored from left to right (left is oldest)
char character_history[50];
uint8_t colour_history[50];
uint8_t character_index = 0;
void save_history(char character, uint8_t colour)
{
    if (character_index < 50)
    {
        character_history[character_index] = character;
        colour_history[character_index] = colour;
    }
    else
    {
        character_index = 49;

        memmove(character_history, character_history + 1, 50 - 1);
        character_history[character_index] = character;

        memmove(colour_history, colour_history + 1, 50 - 1);
        colour_history[character_index] = colour;
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
uint8_t ssd_acc = 0;

int cursor_x = 1;
int cursor_y = 1;
int consecutive_submits = 2;

uint32_t b0_hold_time_ms = 0;
uint32_t b0_release_time_ms = 0;

uint8_t axis = 0;
volatile uint16_t joystick_x = 512;
uint16_t joystick_y = 512;
uint16_t scrollback = 0;

uint16_t joystick_scaler = 0;

uint16_t brightness_acc = 0;
uint8_t brightness_level = 15;
uint8_t prev_brightness_up = 0;
uint8_t prev_brightness_down = 0;

int brightness_down = 0;
int brightness_up = 0;

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
    initialise_joystick();
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
    prev_b0 = PINB & (1 << PB0);
    prev_b1 = PINB & (1 << PB1);
    prev_b2 = PINB & (1 << PB2);
    ledmatrix_clear();
}
void finish_animations()
{

    if (matrix_animation_frame > 0 && scrollback == 0)
    {
        shift_display_left(matrix_animation_frame);
        matrix_animation_frame = 0;
    }
    else if (scrollback > 0)
    {
        draw_character_history(character_history, colour_history, character_index, is_font_large, brightness_level);
        scrollback = 0;
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

void rerender()
{

    finish_animations();
    draw_character_history(character_history, colour_history, character_index, is_font_large, brightness_level);
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
                        buzzer_frequency_animation[buzzer_animation_frame] = 400;
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
                    buzzer_frequency_animation[buzzer_animation_frame] = 550;
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
        draw_char(character, MATRIX_NUM_COLUMNS, COLOUR_YELLOW, is_font_large, brightness_level);
        save_history(character, COLOUR_YELLOW);
        character_index++;
        characters_inputted++;
        matrix_animation_frame = submit_animation_frames[is_font_large];
        consecutive_submits = 1;
        buzzer_animation_frame = 0;
    }
    else if (input == ' ')
    {
        finish_animations();
        current_morse_state = 1;
        marks_inputted = 0;
        led_io_animation_frame += 2;
        led_io_animation <<= 2;
        consecutive_submits = 2;
        cursor_x++;
        printf("\033[1;33m%c", ' ');
        draw_char(' ', MATRIX_NUM_COLUMNS, COLOUR_YELLOW, is_font_large, brightness_level);
        save_history(' ', 0x00);
        character_index++;
        matrix_animation_frame = submit_animation_frames[is_font_large];
    }
}
void render_ssd()
{
    // Render SSD
    ssd_acc = 0;
    if (ssd_mux == 0)
    {
        ssd_mux = 1;
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
        ssd_mux = 0;
        uint8_t decimal_point = marks_inputted > 0;
        ss_render_number(1, characters_inputted, decimal_point);
    }
}
void start_morse(void)
{
    // Clear the serial terminal
    clear_terminal();
    move_terminal_cursor(cursor_x, cursor_y);
    initialise_100ms_timer();
    initialise_scroll_timer();

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
        // PORTD &= ~(0b11 << 2);
        PORTD = (PORTD & ~(0b11 << 2)) | ((led_io_state) & (0b11)) << 2;

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

    save_history(character, COLOUR_RED);

    move_terminal_cursor(cursor_x, cursor_y);
    printf("\033[1;31m%c", character);
    draw_char(character, MATRIX_NUM_COLUMNS, COLOUR_RED, is_font_large, brightness_level);
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
    save_history(character, COLOUR_RED);
    move_terminal_cursor(cursor_x, cursor_y);
    printf("\033[1;31m%c", character);
    draw_char(character, MATRIX_NUM_COLUMNS, COLOUR_RED, is_font_large, brightness_level);
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

        save_history(character, COLOUR_GREEN);
        character_index++;
        move_terminal_cursor(cursor_x, cursor_y);
        printf("\033[1;32m%c", character);
        cursor_x++;
        draw_char(character, MATRIX_NUM_COLUMNS, COLOUR_GREEN, is_font_large, brightness_level);
        characters_inputted++;
        matrix_animation_frame = submit_animation_frames[is_font_large];
    }
    else if (consecutive_submits == 2)
    {
        // Submit word
        printf(" ");

        save_history(' ', 0x00);
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

void handle_inputs(void)
{

    is_font_large = (PIND >> PD5) & 1;
    if (is_font_large && !previous_large)
    {
        rerender();
    }
    else if (!is_font_large && previous_large)
    {
        rerender();
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
        uint16_t frequency = buzzer_frequency_animation[buzzer_animation_frame];
        buzzer_animation_frame++;
        play_piezo(frequency, duty_cycle);
    }
    else
    {
        // stop buzzer
        play_piezo(200, 100);
    }

    int b0_released = !(PINB & (1 << PB0));
    // Count hold times
    int synchronous_mode = (PIND >> PD4) & 1;
    if (synchronous_mode)
    {

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

    brightness_acc += 100;

    if (brightness_acc >= 1000)
    {
        brightness_acc = 0;
        if (brightness_up)
        {
            if (brightness_level < 15)
            {

                brightness_level++;
                rerender();
            }
        }
        if (brightness_down)
        {
            if (brightness_level > 1)
            {
                brightness_level--;
                rerender();
            }
        }
    }
}

ISR(ADC_vect)
{

    // printf("READING ADC");
    // if `axis` is 1, we just read the Y-axis, so allocate the ADC result to `y`
    // otherwise, we just read the X-axis, so allocate the result to `x`

    if (axis)
    {
        joystick_y = ADC;
    }
    else
    {
        joystick_x = ADC;
    }
    float distance;
    if (joystick_x < 512 - DEADZONE)
    {
        distance = 511 - DEADZONE - joystick_x;
    }
    else
    {
        distance = joystick_x - DEADZONE - 512;
    }

    float strength = distance / (511.0 - DEADZONE);
    float timer_modifier = 1 - strength;

    OCR0A = 70 + (uint8_t)(timer_modifier * 184.0);

    brightness_down = joystick_y < (511 - DEADZONE);
    brightness_up = joystick_y >= 512 + DEADZONE;
    if (!brightness_down && !brightness_up)
    {
        brightness_acc = 0;
    }

    if (!prev_brightness_down && brightness_down)
    {
        if (brightness_level > 1)
        {
            brightness_level--;
            rerender();
        }
    }
    if (!prev_brightness_up && brightness_up)
    {
        if (brightness_level < 15)
        {
            brightness_level++;
            rerender();
        }
    }

    prev_brightness_up = brightness_up;
    prev_brightness_down = brightness_down;

    // toggle `axis`, so that the next loop will read the other joystick axis
    axis = axis ? 0 : 1;

    set_joystick_read_axis(axis);
    ADCSRA |= (1 << ADSC);
}
void scroll_left_small_font()
{
    uint8_t stage = scrollback % 4;

    int history_offset = scrollback / 4;
    int history_index = character_index - history_offset;
    if (character_index < history_offset || history_index > 49)
    {
        return;
    }
    scrollback--;
    ledmatrix_shift_left();
    if (stage == 0)
    {
        return;
    }

    char history_letter = character_history[history_index];
    uint8_t colour = colour_history[history_index];

    if (stage != 0)
    {
        uint8_t col = 3 - stage;
        draw_small_char_column(history_letter, MATRIX_NUM_COLUMNS - 1, col, colour, brightness_level);
    }
}

void scroll_left_large_font()
{
    uint8_t stage = (scrollback - 1) % 7;

    int history_offset = (scrollback - 1) / 7;
    int history_index = character_index - history_offset;
    if (character_index < history_offset || history_index > 49)
    {
        return;
    }
    scrollback--;
    ledmatrix_shift_left();
    if (stage == 5 || stage == 6)
    {
        return;
    }

    char history_letter = character_history[history_index];
    if (history_letter == 0)
    {
        return;
    }
    uint8_t colour = colour_history[history_index];

    uint8_t col = 4 - stage;
    draw_large_char_column(history_letter, MATRIX_NUM_COLUMNS - 1, col, colour, brightness_level);
}
void scroll_right_small_font()
{
    scrollback++;
    uint8_t stage = scrollback % 4;

    int history_offset = 4 + scrollback / 4;
    int history_index = character_index - history_offset;
    if (character_index < history_offset || history_index > 49)
    {
        scrollback--;
        return;
    }
    ledmatrix_shift_right();
    if (stage == 0)
    {
        return;
    }

    char history_letter = character_history[history_index];
    uint8_t colour = colour_history[history_index];

    uint8_t col = 3 - stage;
    draw_small_char_column(history_letter, 0, col, colour, brightness_level);
}
void scroll_right_large_font()
{
    scrollback++;
    uint8_t stage = (scrollback + 1) % 7;

    int history_offset = (scrollback + 1) / 7 + 2;
    int history_index = character_index - history_offset;
    if (character_index < history_offset || history_index > 49)
    {
        scrollback--;
        return;
    }
    ledmatrix_shift_right();
    if (stage == 5 || stage == 6)
    {
        return;
    }

    char history_letter = character_history[history_index];
    if (history_letter == 0)
    {
        return;
    }
    uint8_t colour = colour_history[history_index];

    uint8_t col = (4 - stage);
    draw_large_char_column(history_letter, 0, col, colour, brightness_level);
}
ISR(TIMER0_COMPA_vect)
{
    joystick_scaler += 1;
    if (joystick_scaler > 3)
    {
        joystick_scaler = 0;
        if (joystick_x < 511 - DEADZONE && scrollback > 0)
        {
            if (!is_font_large)
            {
                scroll_left_small_font();
            }
            else
            {
                scroll_left_large_font();
            }
        }
        else if (joystick_x >= 512 + DEADZONE)
        {
            if (!is_font_large)
            {
                scroll_right_small_font();
            }
            else
            {
                scroll_right_large_font();
            }
        }
    }
}