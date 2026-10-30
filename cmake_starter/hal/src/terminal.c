// display terminal output 

#include <stdio.h>
#include <stdlib.h> 
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "hal/sampler.h"
#include "hal/periodTimer.h"

static pthread_t terminal_thread_id;
static bool is_initialized = false;

extern int encoder_getFrequency(void);
extern int Sampler_getDips(double* history, int size, double curr_avg);

static void* terminal_thread(void* arg) {
    (void)arg;
    while (is_initialized) {
        Sampler_moveCurrentDataToHistory();

        int size = 0;
        double* history = Sampler_getHistory(&size);
        double avg_light = Sampler_getAverageReading();
        int dips = Sampler_getDips(history, size, avg_light);

        // implement in 1.5
        int pwm = encoder_getFrequency();

        Period_statistics_t stats;
        Period_getStatisticsAndClear(PERIOD_EVENT_SAMPLE_LIGHT, &stats);

        printf("#Smpl/s %3d Flash @ %2dHz avg = %5.3fV dips = %2d  "
               "Smpl ms[%5.3f, %5.3f] avg %5.3f/%d\n",
               size, pwm, avg_light, dips,
               stats.minPeriodInMs, stats.maxPeriodInMs, stats.avgPeriodInMs, stats.numSamples);
        
        if (size > 0) {
            int num_to_print = (size < 10) ? size : 10;
            int step = size / num_to_print;

            for (int i = 0; i < num_to_print; i++) {
                int idx = i * step;
                if (idx >= size) {
                    idx = size - 1;
                    printf("%d:%5.3f\t", idx, history[idx]);

                }
            }

            printf("\n");
        }

        free(history);
        sleep(1);
        
    }

    return NULL;
}

void Terminal_init(void) {
    is_initialized = true;
    pthread_create(&terminal_thread_id, NULL, terminal_thread, NULL);
}

void Terminal_cleanup(void) {
    is_initialized = false;
    pthread_join(terminal_thread_id, NULL);
}