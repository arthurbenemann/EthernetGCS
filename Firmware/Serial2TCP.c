#include "TCPIPConfig.h"
#include "HardwareProfile.h"
#include "TCPIP Stack/TCPIP.h"
#include "Serial2TCP.h"

// Ring buffers for transfering data to and from the UART ISR:
//  - (Head pointer == Tail pointer) is defined as an empty FIFO
//  - (Head pointer == Tail pointer - 1), accounting for wraparound,
//    is defined as a completely full FIFO.  As a result, the max data 
//    in a FIFO is the buffer size - 1.
static BYTE vUARTRXFIFO[65];
static BYTE vUARTTXFIFO[17];
static volatile BYTE *RXHeadPtr = vUARTRXFIFO, *RXTailPtr = vUARTRXFIFO;
static volatile BYTE *TXHeadPtr = vUARTTXFIFO, *TXTailPtr = vUARTTXFIFO;
static BYTE *RXHeadPtrShadow, *RXTailPtrShadow;
static BYTE *TXHeadPtrShadow, *TXTailPtrShadow;

static TCP_SOCKET MySocket1 = INVALID_SOCKET;
static TCP_SOCKET MySocket2 = INVALID_SOCKET;

void receiveTcpDataToTxFifo(TCP_SOCKET MySocket);
void sendRxDataToTcpSockets(TCP_SOCKET MySocket1,TCP_SOCKET MySocket2);
void shadowFifoPointers();
void unshadowFifoPointers();
void initTCPSockets(TCP_SOCKET *MySocket);
void initUART();

/**
 * Exchange data between TCP and the two Serial fifo buffers
 */
void Serial2TCPTask(void) {

    // Make sure to clear UART errors so they don't block all future operations
    if (U2STAbits.OERR)
        U2STAbits.OERR = 0;

    // Reset all buffers if the connection was lost
    if (TCPWasReset(MySocket1)) {};
    if (TCPWasReset(MySocket2)) {};

    int isSocket1Connected = TCPIsConnected(MySocket1);
    int isSocket2Connected = TCPIsConnected(MySocket2);

    // Don't do anything if nobody is connected to us
    if ((!isSocket1Connected)&(!isSocket2Connected)) {
        LED0 = LED_OFF;
        return;
    } else if ((isSocket1Connected) | (isSocket2Connected)) {
        LED0 = LED_ON;
    }

    shadowFifoPointers();

    receiveTcpDataToTxFifo(MySocket1);
    receiveTcpDataToTxFifo(MySocket2);
    sendRxDataToTcpSockets(MySocket1,MySocket2);

    unshadowFifoPointers();

    LED0 =LED_OFF;  // Blinks led durring the duration of this routine
}

/*
 * Write local shadowed FIFO pointers into the volatile FIFO pointers.
 */
void unshadowFifoPointers() {
    IEC1bits.U2RXIE = 0;
    IEC1bits.U2TXIE = 0;

    RXTailPtr = (volatile BYTE*)RXTailPtrShadow;
    TXHeadPtr = (volatile BYTE*)TXHeadPtrShadow;

    IEC1bits.U2RXIE = 1;
    if (TXHeadPtrShadow != TXTailPtrShadow)
        IEC1bits.U2TXIE = 1;
}

/* Read FIFO pointers into a local shadow copy.  Some pointers are volatile
 * (modified in the ISR), so we must do this safely by disabling interrupts
 */
void shadowFifoPointers() {
    RXTailPtrShadow = (BYTE*) RXTailPtr;
    TXHeadPtrShadow = (BYTE*) TXHeadPtr;

    IEC1bits.U2RXIE = 0;
    IEC1bits.U2TXIE = 0;

    RXHeadPtrShadow = (BYTE*) RXHeadPtr;
    TXTailPtrShadow = (BYTE*) TXTailPtr;

    IEC1bits.U2RXIE = 1;
    if (TXHeadPtrShadow != TXTailPtrShadow)
        IEC1bits.U2TXIE = 1;
}

/**
 * Transfer received TCP data into the UART TX FIFO for future transmission (in the ISR)
 */
void receiveTcpDataToTxFifo(TCP_SOCKET MySocket) {
    WORD wMaxPut, wMaxGet, w;
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
}
/*
 *  Transmit pending data that has been placed into the UART RX FIFO (in the ISR)
 */
void sendRxDataToTcpSockets(TCP_SOCKET MySocket1,TCP_SOCKET MySocket2) {
    WORD wMaxPut,wMaxPut1,wMaxPut2, wMaxGet, w;

    wMaxPut1 = TCPIsPutReady(MySocket1); // Get the minimun TCP TX FIFO space
    wMaxPut2 = TCPIsPutReady(MySocket2); 
    wMaxPut = (wMaxPut1>wMaxPut2)?wMaxPut1:wMaxPut2;

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
            TCPPutArray(MySocket1, RXTailPtrShadow, w);
            TCPPutArray(MySocket2, RXTailPtrShadow, w);
            RXTailPtrShadow = vUARTRXFIFO;
            wMaxPut -= w;
        }
        TCPPutArray(MySocket1, RXTailPtrShadow, wMaxPut);
        TCPPutArray(MySocket2, RXTailPtrShadow, wMaxPut);
        RXTailPtrShadow += wMaxPut;

        // No flush.  The stack will automatically flush and do
        // transmit coallescing to minimize the number of TCP
        // packets that get sent.  If you explicitly call TCPFlush()
        // here, latency will go down, but so will max throughput
        // and bandwidth efficiency.
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

/**
 * Sets up the UART peripheral for this application
 */
void Serial2TCPInit(void) {
    initUART();
    initTCPSockets(&MySocket1);
    initTCPSockets(&MySocket2);
    if((MySocket1 == INVALID_SOCKET)||(MySocket2 == INVALID_SOCKET))
        while(1);   // Error when alocation the TCP sockets
}

void initTCPSockets(TCP_SOCKET *MySocket){
    *MySocket = TCPOpen(0, TCP_OPEN_SERVER, Serial2TCP_PORT, TCP_PURPOSE_UART_2_TCP_BRIDGE);
    // Eat the first TCPWasReset() response so we don't
    // infinitely create and reset/destroy client mode sockets
    TCPWasReset(*MySocket);
}

void initUART(){
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

