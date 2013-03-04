#include "TCPIPConfig.h"
#include "HardwareProfile.h"

#define Serial2TCP_PORT	9761
#define BAUD_RATE		57600ul


#include "TCPIP Stack/TCPIP.h"

// Ring buffers for transfering data to and from the UART ISR:
//  - (Head pointer == Tail pointer) is defined as an empty FIFO
//  - (Head pointer == Tail pointer - 1), accounting for wraparound,
//    is defined as a completely full FIFO.  As a result, the max data 
//    in a FIFO is the buffer size - 1.
static BYTE vUARTRXFIFO[65];
static BYTE vUARTTXFIFO[17];
static volatile BYTE *RXHeadPtr = vUARTRXFIFO, *RXTailPtr = vUARTRXFIFO;
static volatile BYTE *TXHeadPtr = vUARTTXFIFO, *TXTailPtr = vUARTTXFIFO;

/**
 * Sets up the UART peripheral for this application
 */
void Serial2TCPInit(void) {
    UMODE = 0x8000; // Set UARTEN.  Note: this must be done before setting UTXEN

    USTA = 0x0400; // UTXEN set
#define CLOSEST_UBRG_VALUE ((GetPeripheralClock()+8ul*BAUD_RATE)/16/BAUD_RATE-1)
#define BAUD_ACTUAL (GetPeripheralClock()/16/(CLOSEST_UBRG_VALUE+1))


#define BAUD_ERROR ((BAUD_ACTUAL > BAUD_RATE) ? BAUD_ACTUAL-BAUD_RATE : BAUD_RATE-BAUD_ACTUAL)
#define BAUD_ERROR_PRECENT	((BAUD_ERROR*100+BAUD_RATE/2)/BAUD_RATE)
#if (BAUD_ERROR_PRECENT > 3)
#warning UART frequency error is worse than 3%
#elif (BAUD_ERROR_PRECENT > 2)
#warning UART frequency error is worse than 2%
#endif

    UBRG = CLOSEST_UBRG_VALUE;

}

/**
 * Exchange data between TCP and the two Serial fifo buffers
 */
void Serial2TCPTask(void) {

    static enum _BridgeState {
        SM_HOME = 0,
        SM_SOCKET_OBTAINED
    } BridgeState = SM_HOME;
    static TCP_SOCKET MySocket = INVALID_SOCKET;
    WORD wMaxPut, wMaxGet, w;
    BYTE *RXHeadPtrShadow, *RXTailPtrShadow;
    BYTE *TXHeadPtrShadow, *TXTailPtrShadow;


    switch (BridgeState) {
        case SM_HOME:
            MySocket = TCPOpen(0, TCP_OPEN_SERVER, Serial2TCP_PORT, TCP_PURPOSE_UART_2_TCP_BRIDGE);

            // Abort operation if no TCP socket of type TCP_PURPOSE_UART_2_TCP_BRIDGE is available
            // If this ever happens, you need to go add one to TCPIPConfig.h
            if (MySocket == INVALID_SOCKET)
                break;

            // Eat the first TCPWasReset() response so we don't
            // infinitely create and reset/destroy client mode sockets
            TCPWasReset(MySocket);

            // We have a socket now, advance to the next state
            BridgeState = SM_SOCKET_OBTAINED;
            break;

        case SM_SOCKET_OBTAINED:
            // Reset all buffers if the connection was lost
            if (TCPWasReset(MySocket)) {
                // Optionally discard anything in the UART FIFOs
                //RXHeadPtr = vUARTRXFIFO;
                //RXTailPtr = vUARTRXFIFO;
                //TXHeadPtr = vUARTTXFIFO;
                //TXTailPtr = vUARTTXFIFO;
            }

            // Don't do anything if nobody is connected to us
            if (!TCPIsConnected(MySocket)) {
                LED0 = LED_OFF;
                break;
            } else {
                LED0 = LED_ON;
            }


            // Make sure to clear UART errors so they don't block all future operations
            if (U2STAbits.OERR)
                U2STAbits.OERR = 0;


            // Read FIFO pointers into a local shadow copy.  Some pointers are volatile
            // (modified in the ISR), so we must do this safely by disabling interrupts
            RXTailPtrShadow = (BYTE*) RXTailPtr;
            TXHeadPtrShadow = (BYTE*) TXHeadPtr;

            IEC1bits.U2RXIE = 0;
            IEC1bits.U2TXIE = 0;

            RXHeadPtrShadow = (BYTE*) RXHeadPtr;
            TXTailPtrShadow = (BYTE*) TXTailPtr;

            IEC1bits.U2RXIE = 1;
            if (TXHeadPtrShadow != TXTailPtrShadow)
                IEC1bits.U2TXIE = 1;

            //
            // Transmit pending data that has been placed into the UART RX FIFO (in the ISR)
            //
            wMaxPut = TCPIsPutReady(MySocket); // Get TCP TX FIFO space
            wMaxGet = RXHeadPtrShadow - RXTailPtrShadow; // Get UART RX FIFO byte count
            if (RXHeadPtrShadow < RXTailPtrShadow)
                wMaxGet += sizeof (vUARTRXFIFO);
            if (wMaxPut > wMaxGet) // Calculate the lesser of the two
                wMaxPut = wMaxGet;
            if (wMaxPut) // See if we can transfer anything
            {
                // Transfer the data over.  Note that a two part put
                // may be needed if the data spans the vUARTRXFIFO
                // end to start address.
                w = vUARTRXFIFO + sizeof (vUARTRXFIFO) - RXTailPtrShadow;
                if (wMaxPut >= w) {
                    TCPPutArray(MySocket, RXTailPtrShadow, w);
                    RXTailPtrShadow = vUARTRXFIFO;
                    wMaxPut -= w;
                }
                TCPPutArray(MySocket, RXTailPtrShadow, wMaxPut);
                RXTailPtrShadow += wMaxPut;

                // No flush.  The stack will automatically flush and do
                // transmit coallescing to minimize the number of TCP
                // packets that get sent.  If you explicitly call TCPFlush()
                // here, latency will go down, but so will max throughput
                // and bandwidth efficiency.
            }

            //
            // Transfer received TCP data into the UART TX FIFO for future transmission (in the ISR)
            //
            wMaxGet = TCPIsGetReady(MySocket); // Get TCP RX FIFO byte count
            wMaxPut = TXTailPtrShadow - TXHeadPtrShadow - 1; // Get UART TX FIFO free space
            if (TXHeadPtrShadow >= TXTailPtrShadow)
                wMaxPut += sizeof (vUARTTXFIFO);
            if (wMaxPut > wMaxGet) // Calculate the lesser of the two
                wMaxPut = wMaxGet;
            if (wMaxPut) // See if we can transfer anything
            {
                // Transfer the data over.  Note that a two part put
                // may be needed if the data spans the vUARTTXFIFO
                // end to start address.
                w = vUARTTXFIFO + sizeof (vUARTTXFIFO) - TXHeadPtrShadow;
                if (wMaxPut >= w) {
                    TCPGetArray(MySocket, TXHeadPtrShadow, w);
                    TXHeadPtrShadow = vUARTTXFIFO;
                    wMaxPut -= w;
                }
                TCPGetArray(MySocket, TXHeadPtrShadow, wMaxPut);
                TXHeadPtrShadow += wMaxPut;
            }

            // Write local shadowed FIFO pointers into the volatile FIFO pointers.

            IEC1bits.U2RXIE = 0;
            IEC1bits.U2TXIE = 0;

            RXTailPtr = (volatile BYTE*)RXTailPtrShadow;
            TXHeadPtr = (volatile BYTE*)TXHeadPtrShadow;

            IEC1bits.U2RXIE = 1;
            if (TXHeadPtrShadow != TXTailPtrShadow)
                IEC1bits.U2TXIE = 1;

            break;
    }
}

/**
 * Copies bytes to and from the local UART TX and RX FIFOs
 */
void _ISR __attribute__((__no_auto_psv__)) _U2RXInterrupt(void) {
    BYTE i;

    // Store a received byte, if pending, if possible
    // Get the byte
    i = U2RXREG;

    // Clear the interrupt flag so we don't keep entering this ISR
    IFS1bits.U2RXIF = 0;

    // Copy the byte into the local FIFO, if it won't cause an overflow
    if (RXHeadPtr != RXTailPtr - 1) {
        if ((RXHeadPtr != vUARTRXFIFO + sizeof (vUARTRXFIFO)) || (RXTailPtr != vUARTRXFIFO)) {
            *RXHeadPtr++ = i;
            if (RXHeadPtr >= vUARTRXFIFO + sizeof (vUARTRXFIFO))
                RXHeadPtr = vUARTRXFIFO;
        }
    }
}

/**
 *  Copies bytes to and from the local UART TX and RX FIFOs
 */
void _ISR __attribute__((__no_auto_psv__)) _U2TXInterrupt(void) {
    // Transmit a byte, if pending, if possible
    if (TXHeadPtr != TXTailPtr) {
        // Clear the TX interrupt flag before transmitting again
        IFS1bits.U2TXIF = 0;

        U2TXREG = *TXTailPtr++;
        if (TXTailPtr >= vUARTTXFIFO + sizeof (vUARTTXFIFO))
            TXTailPtr = vUARTTXFIFO;
    } else // Disable the TX interrupt if we are done so that we don't keep entering this ISR
    {
        IEC1bits.U2TXIE = 0;
    }
}




