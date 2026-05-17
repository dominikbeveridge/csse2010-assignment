

#include <avr/io.h>
#define BUZZER_F_CPU 8000000UL // 8MHz
#include <stdint.h>

#define PRESCALER 256UL

uint8_t freq_to_ocr2a(uint16_t freq)
{
    uint32_t timer_clock = BUZZER_F_CPU / PRESCALER;
    uint32_t top = (timer_clock / freq) - 1;

    if (top > 255)
    {
        top = 255;
    }

    return (uint8_t)top;
}

uint8_t duty_cycle_to_ocr2b(float dutycycle, uint8_t top)
{
    uint16_t period_counts = top + 1;
    uint16_t pulse_counts = (dutycycle * period_counts) / 100.0;

    if (pulse_counts == 0)
    {
        return 0;
    }

    if (pulse_counts > period_counts)
    {
        pulse_counts = period_counts;
    }

    return pulse_counts - 1;
}

void initialise_piezo(void)

{
    uint16_t freq = 200;
    float dutycycle = 100;

    uint8_t top = freq_to_ocr2a(freq);
    uint8_t pulse = duty_cycle_to_ocr2b(dutycycle, top);

    // Make OC2B pin an output.
    // Check your AVR datasheet: on some AVRs OC2B is not PD6.
    DDRD |= (1 << PD6);

    OCR2A = top;
    OCR2B = pulse;

    // Fast PWM, TOP = OCR2A
    // OC2B non-inverting mode
    TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);

    // WGM22 = 1, prescaler = /256
    TCCR2B = (1 << WGM22) | (1 << CS22) | (1 << CS21);
}

void play_piezo(uint16_t freq, float dutycycle)
{
    if (dutycycle == 100)
    {
        TCCR2B = 0;
    }
    else
    {
        // WGM22 = 1, prescaler = /256
        TCCR2B = (1 << WGM22) | (1 << CS22) | (1 << CS21);
    }

    uint8_t top = freq_to_ocr2a(freq);
    uint8_t pulse = duty_cycle_to_ocr2b(dutycycle, top);

    OCR2A = top;
    OCR2B = pulse;
}