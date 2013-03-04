#include "Compiler.h"

// PIC24FJ256GB106 picDev
_CONFIG2(PLLDIV_DIV3 & PLL_96MHZ_ON & FNOSC_PRIPLL & IOL1WAY_OFF & POSCMOD_XT); // Primary XT OSC with 96MHz PLL (12MHz crystal input), IOLOCK can be set and cleared
_CONFIG1(JTAGEN_OFF & ICS_PGx2 & FWDTEN_OFF);

// C30  Exception Handlers
// If your code gets here, you either tried to read or write
// a NULL pointer, or your application overflowed the stack
// by having too many local variables or parameters declared.

void _ISR __attribute__((__no_auto_psv__)) _AddressError(void) {
    Nop();
    Nop();
}
void _ISR __attribute__((__no_auto_psv__)) _StackError(void) {
    Nop();
    Nop();
}

