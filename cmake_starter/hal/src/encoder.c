#include "hal/encoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <gpiod.h>
#include <time.h>
#include "hal/pwm.h"
#include <stdint.h>

#define MIN_FREQ 0
#define MAX_FREQ 500
#define INIT_FREQ 10
#define DEBOUNCE_US 3000             
#define MIN_CLICK_GAP_NS 5000000ULL


static struct gpiod_chip *chip = NULL;
static struct gpiod_line_request *request = NULL; 
static int pinA_offset = -1;                     
static int pinB_offset = -1;

static pthread_t encoderThread;
pthread_mutex_t freqMutex = PTHREAD_MUTEX_INITIALIZER;
static int frequency = INIT_FREQ;
static volatile bool running = true;

static void updateFrequency(int change) {
    pthread_mutex_lock(&freqMutex);
    int newFrequency = frequency + change;
    if (newFrequency >= MIN_FREQ && newFrequency <= MAX_FREQ) {
        frequency = newFrequency;
        PWM_setFreq(frequency);
    }
    pthread_mutex_unlock(&freqMutex);
}

static void* encoderThreadLoop(void* arg) {
    (void)arg;
    struct gpiod_edge_event_buffer *buffer = gpiod_edge_event_buffer_new(16);
    if (!buffer) {
        perror("gpiod_edge_event_buffer_new");
        return NULL;
    }

    int levelB = (gpiod_line_request_get_value(request, pinB_offset) == GPIOD_LINE_VALUE_ACTIVE);
    uint64_t lastClickNs = 0;

    while (running) {
        int waitResult = gpiod_line_request_wait_edge_events(request, 100000000);
        if (waitResult < 0) {
            perror("gpiod_line_request_wait_edge_events");
            break;
        } else if (waitResult == 0) {
            continue;
        }

        int num_events = gpiod_line_request_read_edge_events(request, buffer, 16);
        if (num_events < 0) {
            perror("gpiod_line_request_read_edge_events");
            continue;
        }

        for (int i = 0; i < num_events; i++) {
            struct gpiod_edge_event *event = gpiod_edge_event_buffer_get_event(buffer, i);
            if (!event) continue;

            int offset = (int)gpiod_edge_event_get_line_offset(event);
            enum gpiod_edge_event_type type = gpiod_edge_event_get_event_type(event);

            if (offset == pinB_offset) {
                levelB = (type == GPIOD_EDGE_EVENT_RISING_EDGE);
            } else if (offset == pinA_offset && type == GPIOD_EDGE_EVENT_FALLING_EDGE) {
                uint64_t nowNs = gpiod_edge_event_get_timestamp_ns(event);
                if (nowNs - lastClickNs < MIN_CLICK_GAP_NS) {
                    continue;
                }
                lastClickNs = nowNs;

                if (levelB) {
                    updateFrequency(1);
                } else {
                    updateFrequency(-1);
                }
            }
        }
    }
    gpiod_edge_event_buffer_free(buffer);
    return NULL;
}

void encoder_init(int pinA, int pinB) {
    pinA_offset = pinA;
    pinB_offset = pinB;

    chip = gpiod_chip_open("/dev/gpiochip3");
    if (!chip) {
        perror("Failed to open gpiochip3");
        exit(EXIT_FAILURE);
    }

    struct gpiod_line_settings *settingsA = gpiod_line_settings_new();
    struct gpiod_line_settings *settingsB = gpiod_line_settings_new();
    struct gpiod_line_config *line_config = gpiod_line_config_new();

    if (!settingsA || !settingsB || !line_config) {
        perror("Failed to allocate line settings or config");
        exit(EXIT_FAILURE);
    }

    gpiod_line_settings_set_direction(settingsA, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(settingsA, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_direction(settingsB, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(settingsB, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_debounce_period_us(settingsA, DEBOUNCE_US);
    gpiod_line_settings_set_debounce_period_us(settingsB, DEBOUNCE_US);

    gpiod_line_settings_set_bias(settingsA, GPIOD_LINE_BIAS_PULL_UP);
    gpiod_line_settings_set_bias(settingsB, GPIOD_LINE_BIAS_PULL_UP);

    uint32_t offsetA = pinA;
    uint32_t offsetB = pinB;
    gpiod_line_config_add_line_settings(line_config, &offsetA, 1, settingsA);
    gpiod_line_config_add_line_settings(line_config, &offsetB, 1, settingsB);

    request = gpiod_chip_request_lines(chip, NULL, line_config);

    gpiod_line_settings_free(settingsA);
    gpiod_line_settings_free(settingsB);
    gpiod_line_config_free(line_config);

    if (!request) {
        perror("Failed to request lines");
        exit(EXIT_FAILURE);
    }

    running = true;
    if (pthread_create(&encoderThread, NULL, encoderThreadLoop, NULL) != 0) {
        perror("Failed to create encoder thread");
        exit(EXIT_FAILURE);
    }
}

void encoder_cleanup(void) {
    running = false;
    pthread_join(encoderThread, NULL);

    if (request) {
        gpiod_line_request_release(request);
    }

    if (chip) {
        gpiod_chip_close(chip);
    }
}

int encoder_getFrequency(void) {
    pthread_mutex_lock(&freqMutex);
    int freq = frequency;
    pthread_mutex_unlock(&freqMutex);
    return freq;
}