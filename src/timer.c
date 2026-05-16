
#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

uint16_t calculate_ocr(uint16_t frequency, uint16_t prescaler)
{
    return (uint16_t)(8000000UL / (prescaler * frequency) - 1);
}

void initialise_100ms_timer()
{
    // CTC Mode, with prescaler of 64
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    // period 100ms or 0.1s. frequency = 10
    OCR1A = calculate_ocr(10, 64); // should be 12,499
    // enable timer interrupt
    TIMSK1 = (1 << OCIE1A);
}
