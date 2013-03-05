//------------------------------------------------
//	RC SERVO library
//	Arthur Benemann 19/12/2011
//------------------------------------------------

#include "Compiler.h"
#include "RC.h"

void initRC() {

    T5CON = 0x8010; // Timer 5 ON, Internal OSC, 1:8 prescaler , 0.5usec tick
    PR5 = 2000; // Timer period

    OC4R = DEFAULT_SERVO_VALUE * 2u; // Starting servo value 1.5ms
    OC4RS = SERVO_UPDATE_PERIOD * 2u; // Period 20ms
    OC4CON2 = 0x101f; // Timer 5 as Source
    OC4CON1 = 0x0C07;

    OC5R = DEFAULT_SERVO_VALUE * 2u; // Starting servo value 1.5ms
    OC5RS = SERVO_UPDATE_PERIOD * 2u; // Period 20ms
    OC5CON2 = 0x101f; // Timer 5 as Source
    OC5CON1 = 0x0C07;
}

//----------------------------------------------

void writeRC(enum RC_CH ch, unsigned int position) {
    if (position > MAX_SERVO_VALUE) position = MAX_SERVO_VALUE;
    if (position < MIN_SERVO_VALUE) position = MIN_SERVO_VALUE;
    switch (ch){
        case RC_CH1:
            OC4R = position <<1; // Duty cycle
            break;
        case RC_CH2:
            OC5R = position <<1; // Duty cycle
            break;
    }
}
