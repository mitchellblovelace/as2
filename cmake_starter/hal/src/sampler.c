#include "hal/sampler.h"
#include "hal/adc.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "hal/periodTimer.h"
#include <assert.h>


#define MAX_SAMPLES_PER_SECOND 2000

static void* sampling_thread(void *arg);

static pthread_t sampling_thread_id;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;


// shared data 

static double curr_buffer[MAX_SAMPLES_PER_SECOND];
static int curr_count = 0;

static double hist_buffer[MAX_SAMPLES_PER_SECOND];
static int hist_count = 0;

static double exp_avg = 0.0;
static long long total_samples = 0;


static bool is_initialized = false;

void Sampler_init(void) 
{
    is_initialized = true;
    int res = pthread_create(&sampling_thread_id, NULL, sampling_thread, NULL);

    assert(res == 0);

}

void Sampler_cleanup(void) 
{
    is_initialized = false;
    int res = pthread_join(sampling_thread_id, NULL);
    assert(res == 0);
}

void Sampler_moveCurrentDataToHistory(void) 
{
    pthread_mutex_lock(&data_mutex);

    // copy current buffer to history buffer
    memcpy(hist_buffer, curr_buffer, curr_count * sizeof(double));
    hist_count = curr_count;

    // reset current buffer
    curr_count = 0;

    pthread_mutex_unlock(&data_mutex);
}

int Sampler_getHistorySize(void) 
{
    // return # of samples collected during previous complete second 
    pthread_mutex_lock(&data_mutex);
    int size = hist_count;
    pthread_mutex_unlock(&data_mutex);
    return size;
}

double* Sampler_getHistory(int *size) {

    // return copy of the samples in the sample history
    pthread_mutex_lock(&data_mutex);

    *size = hist_count;

    // allocate memory for the history array and copy the data
    double *history = malloc(*size * sizeof(double));
    if (history) {
        memcpy(history, hist_buffer, *size * sizeof(double));
    }
    pthread_mutex_unlock(&data_mutex);
    return history;
}

double Sampler_getAverageReading(void) 
{
    // Return the average light level (not tied to the history).
    pthread_mutex_lock(&data_mutex);
    double avg = exp_avg;
    pthread_mutex_unlock(&data_mutex);
    return avg;
}

long long Sampler_getNumSamplesTaken(void) 
{
    // Return the total number of light level samples taken so far.
    pthread_mutex_lock(&data_mutex);
    long long num_samples = total_samples;
    pthread_mutex_unlock(&data_mutex);
    return num_samples; 
}

static void* sampling_thread(void *arg) 
{
    (void)arg;
    struct timespec sleep_time = {0, 500000}; // 0.5ms sleep/wait time 
    while(is_initialized) {
        Period_markEvent(PERIOD_EVENT_SAMPLE_LIGHT);

        // read current light level
        double curr_light = ADC_getVoltageReading();

        pthread_mutex_lock(&data_mutex);

        // exponential smoothing implementation 

        if (total_samples == 0) {
            exp_avg = curr_light;
        } else {
            exp_avg = 0.9 * exp_avg + 0.001 * curr_light;
        }

        // store sample in current buffer 
        if (curr_count < MAX_SAMPLES_PER_SECOND) {
            curr_buffer[curr_count] = curr_light;
            curr_count++;
        }

        total_samples++;

        pthread_mutex_unlock(&data_mutex);

        nanosleep(&sleep_time, NULL); // sleep for 0.5ms   


    }

    return NULL;
}

int Sampler_getDips(double* history, int size, double curr_avg) 
{
    // Count the number of dips in the history compared to the current average.
    int dips = 0;
    bool in_dip = false;


    for (int i = 0; i < size; i++) {
        double sample = history[i];

        if (!in_dip && sample <= (curr_avg - 0.1)) {
            dips++;
            in_dip = true;
        } else if (in_dip && sample > (curr_avg - 0.1)) {
            in_dip = false;
        }
    }
    return dips;
}

