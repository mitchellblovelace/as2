#include "hal/pwm.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <gpiod.h>
#include <time.h>

#define LED_CHIP_PATH "/dev/gpiochip3"
#define LED_LINE_OFFSET 16

#define NS_PER_SEC 1000000000L
#define DEFAULT_FREQ 10

static struct gpiod_chip *chip = NULL;
static struct gpiod_line_request *request = NULL;

static pthread_t ledThread;
static pthread_mutex_t freqMutex = PTHREAD_MUTEX_INITIALIZER;
static int frequency = 0;
static volatile bool running = false;

static void set_led(bool on) {
    gpiod_line_request_set_value(request, LED_LINE_OFFSET, on ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

static void add_ns(struct timespec *t, long ns) {
    t->tv_nsec += ns;
    while (t->tv_nsec >= NS_PER_SEC) {
        t->tv_nsec -= NS_PER_SEC;
        t->tv_sec++;
    }
}

static void* ledThreadLoop(void* arg) {
    (void)arg;
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    bool ledOn = false;
    int lastFrequency = -1;

    while(running) {
        pthread_mutex_lock(&freqMutex);
        int hz = frequency;
        pthread_mutex_unlock(&freqMutex);

        if (hz <= 0) {
            if (ledOn) {
                set_led(false);
                ledOn = false;
            }
            struct timespec pause = {0, 20 * 1000000L};
            nanosleep(&pause, NULL);
            continue;
        }

        if (hz != lastFrequency) {
            clock_gettime(CLOCK_MONOTONIC, &next);
            lastFrequency = hz;
        }

        ledOn = !ledOn;
        set_led(ledOn);
        add_ns(&next, NS_PER_SEC / (2 * hz));
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    set_led(false);
    return NULL;
}

void PWM_init(void) {
    chip = gpiod_chip_open(LED_CHIP_PATH);
    if (!chip) {
        perror("Failed to open gpio chip for led");
        exit(EXIT_FAILURE);
    }

    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    struct gpiod_line_config *line_config = gpiod_line_config_new();
    if (!settings || !line_config) {
        perror("Failed to allocate led line settings or config");
        exit(EXIT_FAILURE);
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    unsigned int offset = LED_LINE_OFFSET;
    gpiod_line_config_add_line_settings(line_config, &offset, 1, settings);

    request = gpiod_chip_request_lines(chip, NULL, line_config);

    gpiod_line_settings_free(settings);
    gpiod_line_config_free(line_config);

    if (!request) {
        perror("Failed to request led line");
        exit(EXIT_FAILURE);
    }

    frequency = DEFAULT_FREQ;

    running = true;
    if (pthread_create(&ledThread, NULL, ledThreadLoop, NULL) != 0) {
        perror("Failed to create led thread");
        exit(EXIT_FAILURE);
    }
}

void PWM_setFreq(int hz) {
    if (hz < 0) {
        hz = 0;
    }

    pthread_mutex_lock(&freqMutex);
    if (hz != frequency) {
        frequency = hz;
    }
    pthread_mutex_unlock(&freqMutex);
}

void PWM_cleanup(void) {
    running = false;
    pthread_join(ledThread, NULL);

    if (request) {
        gpiod_line_request_release(request);
    }

    if (chip) {
        gpiod_chip_close(chip);
    }
}