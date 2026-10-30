#include "hal/pwm.h"
#include <stdio.h>
#include <stdlib.h>

#define PWM_DIR "/sys/class/pwm/pwmchip0/pwm0"

static void write_to_file(const char* path, const char* value) {
    FILE* f = fopen(path, "w");
    if (!f) {
        perror("Unable to open file");
        exit(1);
    }
    fprintf(f, "%s", value);
    fclose(f);
}

void PWM_init(void) {
    PWM_setFreq(10); // default frequency 10 Hz per assignment requirements 

    char path[128];
    snprintf(path, sizeof(path), "%s/enable", PWM_DIR);
    write_to_file(path, "1");
}

void PWM_setFreq(int hz) {
    char period_path[128], duty_cycle_path[128], enable_path[128];

    snprintf(period_path, sizeof(period_path), "%s/period", PWM_DIR);
    snprintf(duty_cycle_path, sizeof(duty_cycle_path), "%s/duty_cycle", PWM_DIR);
    snprintf(enable_path, sizeof(enable_path), "%s/enable", PWM_DIR);

    if (hz <= 0) {
        // turn off led 
        write_to_file(enable_path, "0");
        return;
    }

    int period_ns = 1000000000 / hz; // period in nanoseconds
    int duty_cycle_ns = period_ns / 2; // 50% duty cycle

    char buffer1[32], buffer2[32];
    sprintf(buffer1, "%d", period_ns);
    sprintf(buffer2, "%d", duty_cycle_ns);
    
    write_to_file(period_path, buffer1);

    write_to_file(duty_cycle_path, "0");
    write_to_file(period_path, buffer1); // write twice to ensure proper update
    write_to_file(duty_cycle_path, buffer2); // write twice to ensure proper update


}

void PWM_cleanup(void) {
    char enable_path[128];
    snprintf(enable_path, sizeof(enable_path), "%s/enable", PWM_DIR);
    write_to_file(enable_path, "0"); // disable PWM
}