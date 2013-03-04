/**
 *  Hardware Definitions
 */
#include "Compiler.h"

// Clock frequency values
#define MAXIMUM_PIC_FREQ		(32000000ul)
// These directly influence timed events using the Tick module.  They also are used for UART and SPI baud rate generation.
#define GetSystemClock()		(MAXIMUM_PIC_FREQ)			// Hz
#define GetInstructionClock()	(GetSystemClock()/2)	// Might need changing if using Doze modes.
#define GetPeripheralClock()	(GetSystemClock()/2)	// Divisor may be different if using a PIC32 since it's configurable.

// Hardware I/O pin mappings
#define LED0_TRIS			_TRISD1
#define LED0				(_RD1)
#define LED_ON                          1
#define LED_OFF                         0


// ENC28J60 I/O pins
#define ENC_CS_TRIS			(TRISEbits.TRISE7)	// Comment this line out if you are using the ENC424J600/624J600, MRF24WB0M, or other network controller.
#define ENC_CS_IO			(LATEbits.LATE7)
#define ENC_SCK                         (_RP19R)    // PIC output
#define ENC_SDI                         (_RP26R)    // PIC output
#define ENC_SDO                         (21)        // PIC input
// SPI SCK, SDI, SDO pins are automatically controlled by the
// PIC24/dsPIC SPI module
#define ENC_SPI_IF			(IFS0bits.SPI1IF)
#define ENC_SSPBUF			(SPI1BUF)
#define ENC_SPISTAT			(SPI1STAT)
#define ENC_SPISTATbits		(SPI1STATbits)
#define ENC_SPICON1			(SPI1CON1)
#define ENC_SPICON1bits		(SPI1CON1bits)
#define ENC_SPICON2			(SPI1CON2)


//UART PPS pins
#define PIC_TX_OD (_ODF5)   // PIC OpenDrain for TX pin
#define PIC_TX  (_RP17R)    // PIC output
#define PIC_RX  (10)        // PIC input
// Select which UART to use.
#define UBRG				U2BRG
#define UMODE				U2MODE
#define USTA				U2STA

// PPS definitions
#define SCK1    8
#define SDO1    7
#define UART2   5