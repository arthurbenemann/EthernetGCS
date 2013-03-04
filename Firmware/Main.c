// Include all headers for any enabled TCPIP Stack functions
#include "TCPIP Stack/TCPIP.h"

#include "Serial2TCP.h"

// Declare AppConfig structure and some other supporting stack variables
APP_CONFIG AppConfig;


// Private helper functions.
static void InitStackConfig(void);
static void InitializeBoard(void);


/**
* Main application entry point.
*/
int main(void) {
    InitializeBoard();
    TickInit();
    InitStackConfig();
    StackInit();
    Serial2TCPInit();

    while (1) {
        StackTask();
        StackApplications();
        Serial2TCPTask();
    }
}

/**
 *   This routine initializes the hardware.  It is a generic initialization
 *   routine for many of the Microchip development boards, using definitions
 *   in HardwareProfile.h to determine specific initialization.
 */
static void InitializeBoard(void) {
    CLKDIVbits.RCDIV = 0; // Set 1:1 8MHz FRC postscalar
    __builtin_write_OSCCONL(OSCCON & 0xBF); // Unlock PPS

    // Configure ENC28J60 SPI1 PPS pins
    ENC_CS_IO = 1;
    ENC_CS_TRIS = 0;

    _RP19R = 8; // Assign RP19 to SCK1 (output)
    _RP26R = 7; // Assign RP26 to SDO1 (output)
    _SDI1R = 21; // Assign RP21 to SDI1 (input)


    // Configure UART2 PPS pins
    _U2RXR = 10; // Assign RF4/RP10 to U2RX (input)
    _RP17R = 5; // Assign RF5/RP17 to U2TX (output)


    __builtin_write_OSCCONL(OSCCON | 0x40); // Lock PPS
}

/**
 * Initilize stack variables
 */
static ROM BYTE SerializedMACAddress[6] = {MY_DEFAULT_MAC_BYTE1, MY_DEFAULT_MAC_BYTE2, MY_DEFAULT_MAC_BYTE3, MY_DEFAULT_MAC_BYTE4, MY_DEFAULT_MAC_BYTE5, MY_DEFAULT_MAC_BYTE6};

static void InitStackConfig(void) {

        // Start out zeroing all AppConfig bytes to ensure all fields are
        // deterministic for checksum generation
        memset((void*) &AppConfig, 0x00, sizeof (AppConfig));

        AppConfig.Flags.bIsDHCPEnabled = TRUE;
        AppConfig.Flags.bInConfigMode = TRUE;
        memcpypgm2ram((void*) &AppConfig.MyMACAddr, (ROM void*) SerializedMACAddress, sizeof (AppConfig.MyMACAddr));

        AppConfig.MyIPAddr.Val = MY_DEFAULT_IP_ADDR_BYTE1 | MY_DEFAULT_IP_ADDR_BYTE2 << 8ul | MY_DEFAULT_IP_ADDR_BYTE3 << 16ul | MY_DEFAULT_IP_ADDR_BYTE4 << 24ul;
        AppConfig.DefaultIPAddr.Val = AppConfig.MyIPAddr.Val;
        AppConfig.MyMask.Val = MY_DEFAULT_MASK_BYTE1 | MY_DEFAULT_MASK_BYTE2 << 8ul | MY_DEFAULT_MASK_BYTE3 << 16ul | MY_DEFAULT_MASK_BYTE4 << 24ul;
        AppConfig.DefaultMask.Val = AppConfig.MyMask.Val;
        AppConfig.MyGateway.Val = MY_DEFAULT_GATE_BYTE1 | MY_DEFAULT_GATE_BYTE2 << 8ul | MY_DEFAULT_GATE_BYTE3 << 16ul | MY_DEFAULT_GATE_BYTE4 << 24ul;
        AppConfig.PrimaryDNSServer.Val = MY_DEFAULT_PRIMARY_DNS_BYTE1 | MY_DEFAULT_PRIMARY_DNS_BYTE2 << 8ul | MY_DEFAULT_PRIMARY_DNS_BYTE3 << 16ul | MY_DEFAULT_PRIMARY_DNS_BYTE4 << 24ul;
        AppConfig.SecondaryDNSServer.Val = MY_DEFAULT_SECONDARY_DNS_BYTE1 | MY_DEFAULT_SECONDARY_DNS_BYTE2 << 8ul | MY_DEFAULT_SECONDARY_DNS_BYTE3 << 16ul | MY_DEFAULT_SECONDARY_DNS_BYTE4 << 24ul;

        // Load the default NetBIOS Host Name
        memcpypgm2ram(AppConfig.NetBIOSName, (ROM void*) MY_DEFAULT_HOST_NAME, 16);
        FormatNetBIOSName(AppConfig.NetBIOSName);

}
