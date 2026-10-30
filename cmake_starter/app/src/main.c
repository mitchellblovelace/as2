// Main program to build the application
// Has main(); does initialization and cleanup and perhaps some basic logic.

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "hal/adc.h"
#include "hal/pwm.h"
#include "hal/sampler.h"
#include "hal/udp.h"
#include "hal/encoder.h"
#include "hal/periodTimer.h"
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

// #include "hal/periodTimer.h" when implemented
// rotary and terminal not implemented yet...
extern void Terminal_init(void);
extern void Terminal_cleanup(void);

bool program_running = true;

#define ENCODER_PIN_A 20
#define ENCODER_PIN_B 21


int main()
{
    Period_init();

    ADC_init();
    PWM_init();

    encoder_init(ENCODER_PIN_A, ENCODER_PIN_B);
    Sampler_init();
    Terminal_init();
    UDP_init();

    while (program_running) {
        sleep(0.1);

        int targetHz = encoder_getFrequency();
        PWM_setFreq(targetHz);
    }
    
    UDP_cleanup();
    Terminal_cleanup();
    Sampler_cleanup();
    encoder_cleanup();
    PWM_cleanup();
    ADC_cleanup();
    Period_cleanup();

    return 0;
}