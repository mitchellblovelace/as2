#ifndef ENCODER_H
#define ENCODER_H

void encoder_init(int pinA, int pinB);

void encoder_cleanup(void);

int encoder_getFrequency(void);

#endif
