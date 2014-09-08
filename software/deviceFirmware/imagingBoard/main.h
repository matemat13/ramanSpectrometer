/*struct ro_packet    //data type for serialized output of data
{
 short pixel_number;
 short pixel_value;
 unsigned char checksum;
};*/


enum readOutState
{
 ro_begin,
 ro_active,
 ro_leadingDummy,
 ro_leadingShielded,
 ro_headerElements,
 ro_signalElements,
 ro_trailingDummy,
 ro_integrationTime,
 ro_finish,
 ro_idle
};
/*
enum modeType
{
 mode_textual,
 mode_binary
};*/

// sensitivity
// this determines the time (in us) the CCD will wait after each reading
int none        = 0;
int veryLow     = 1;
int low         = 100;
int mediumLow   = 1000;
int medium      = 100000;
int high        = 1000000;
int veryHigh    = 10000000;

int pixelTotal              = 3694;
int leadingDummyElements    = 16;
int leadShieldedElements    = 13;
int headerElements          = 3;
const int signalElements    = 3648;
int trailingDummyElements   = 14;


#define MV(x) ((0xFFF*x)/3300)

#define BYTE_ORDERING "little endian"
#define ADC_BIT_COUNT 12
#define SW_VERSION 0.1

#define BAUDRATE 230400//460800//921600//230400//115200
//#define PACKET_LENGTH 5