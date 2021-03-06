// Include all headers for any enabled TCPIP Stack functions
#include "TCPIP Stack/TCPIP.h"

#include "Serial2TCP.h"
#include "RC.h"

// Declare AppConfig structure and some other supporting stack variables
APP_CONFIG AppConfig;


// Private helper functions.
static void InitStackConfig(void);
static void InitializeBoard(void);
void updateRC();


/**
* Main application entry point.
*/
int main(void) {
    

    InitializeBoard();
    initRC();
    TickInit();
    InitStackConfig();
    StackInit();
    Serial2TCPInit();


    while (1) {
        StackTask();
        StackApplications();
        Serial2TCPTask();
        updateRC();
    }
}


/**
 * Just a demo the moves the servos in oposite directions.
 */
void updateRC(){
    static int i = 0;
    static int delay =0;
    if(delay-- <0){
        delay = 10;
    writeRC(RC_CH1,DEFAULT_SERVO_VALUE + i);
        writeRC(RC_CH2,DEFAULT_SERVO_VALUE - i);
        if(++i>400) i =-500;
    }
}

/**
 *   This routine initializes the hardware.
 */
static void InitializeBoard(void) {
    CLKDIVbits.RCDIV = 0; // Set 1:1 8MHz FRC postscalar
    __builtin_write_OSCCONL(OSCCON & 0xBF); // Unlock PPS

    LED0 = LED_OFF;
    LED0_TRIS = 0;

    // Configure ENC28J60 SPI1 PPS pins
    ENC_CS_IO = 1;
    ENC_CS_TRIS = 0;
    ENC_SCK = SCK1;
    ENC_SDI = SDO1;
    _SDI1R = ENC_SDO;

    // Configure UART2 PPS pins
    PIC_TX_OD = 1;  // Enable Open Drain on TX pin to suport 5V output
    PIC_TX = UART2;
    _U2RXR = PIC_RX;

    // Configure Servo pins
    RC1_OUT = OC5;
    RC2_OUT = OC4;


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
