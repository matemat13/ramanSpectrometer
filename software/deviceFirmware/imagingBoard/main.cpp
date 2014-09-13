#include "mbed.h"

#include "main.h"
//  ***   BE SURE TO TEST BEFORE AND AFTER FOR 'IDLING'... CLEAR THE CCD EVERY OTHER FRAME TO TRY AND ELIMINATE NOISE BUILDUP
//From http://www.ing.iac.es/~docs/ins/das/ins-das-29/integration.html
//"Idling" vs. integrating
//Charge from light leaks and thermal noise builds up on the detector between observations. If this charge is integrated by the detector, it may not be completely
//cleared away by the clear cycle of the next observation. In that case, the observation will be contaminated by extra counts. (Often, this appears as a ramp in the
//background leading up to a saturated region in the low-numbered rows.)
//To avoid this problem, the detector is made to clear itself continuously between observations. This is called "idling", and is reported as such on the mimic.



// Definitions of pin managing objects
PwmOut masterClock(PB_4);
PwmOut shiftGate(PB_8);
InterruptIn shiftGate_int(PC_8);
DigitalOut ICG(PB_3);
AnalogIn imageIn(A1);
DigitalOut LED(LED1);
Serial raspi(USBTX, USBRX);

/* NOT IN THE CCDs SPECS
int masterFreq_period       = 20;      //microseconds
int masterFreq_width        = 10;      //microseconds
int shiftGate_period        = 66;    //microseconds
int shiftGate_width         = 33;    //microseconds
*/

// Much closer to specs
int masterFreq_period       = 2;      //microseconds
int masterFreq_width        = 1;      //microseconds
int shiftGate_period        = 8;    //microseconds
int shiftGate_width         = 4;    //microseconds

int sensitivity = mediumLow;
int pixelCount; //the current count of pixels read
readOutState state; //current state of sample readout
//modeType mode;  //not used

short pixelValue[signalElements];
const char *b_pixelValue = (char*)pixelValue;   //just a byte pointer for binary access to the data
/*UNUSED
unsigned char calculate_checksum(ro_packet packet)
{
 return (((unsigned char*)&(packet.pixel_number))[0] + ((unsigned char*)&(packet.pixel_number))[1] + ((unsigned char*)&(packet.pixel_value))[0] + ((unsigned char*)&(packet.pixel_value))[1] + 1)%256;
}

void send_packet(ro_packet packet)
{
 raspi.putc(((char*)&(packet.pixel_number))[0]);
 raspi.putc(((char*)&(packet.pixel_number))[1]);
 raspi.putc(((char*)&(packet.pixel_value))[0]);
 raspi.putc(((char*)&(packet.pixel_value))[1]);
 raspi.putc(packet.checksum);
}

short readShort()
{
 //little endian!
 return raspi.getc() + raspi.getc()*256;
}*/

//sends the buffered data over serial port in chunks
void send_data()
{
 LED = 1;
 bool all_sent = false;
 int chunk_len = 180;
 int total_len = 7296;
 int byte_n = 0;
 int to_send = 0;
 int oldsent;
 
 for (char chunk_n = 0; chunk_n < total_len/chunk_len+1; chunk_n++)
 {
     oldsent = to_send;
     if (chunk_n == total_len/chunk_len)
         to_send = total_len;
     else
         to_send += chunk_len;
     all_sent = false;
     
     while (!all_sent)
     {
      raspi.putc(chunk_n);
      for (; byte_n < to_send; byte_n++)
      {
       raspi.putc(b_pixelValue[byte_n]);
      }
      if (raspi.getc() == 'y')
      {
       all_sent = true;
      } else
      {
       byte_n = oldsent;
      }
     }
 }
 LED = 0;
}

//main sampling function
void checkState()
{
    shiftGate_int.disable_irq();
    switch (state) {
        case ro_begin:
            ICG = 0;
            //readOutTrigger = 1;
            state = ro_active;
            LED = 1;
            break;
        case ro_active:
            ICG = 1;
            state = ro_leadingDummy;
            break;
        case ro_leadingDummy:
            pixelCount++;
            if (pixelCount == leadingDummyElements-1)   //we count from 0 to leadingDummyElements-1
            {
             pixelCount = 0;
             state = ro_leadingShielded;
            }
            break;
        case ro_leadingShielded:
            pixelCount++;
            if (pixelCount == leadShieldedElements-1)
            {
             pixelCount = 0;
             state = ro_headerElements;
            }
            break;
        case ro_headerElements:
            pixelCount++;
            if (pixelCount == headerElements-1)
            {
             pixelCount = 0;
             state = ro_signalElements;
            }
            break;
        case ro_signalElements:
            pixelCount++;
            pixelValue[pixelCount] = imageIn.read_u16();
            if (pixelCount == signalElements-1) //important!
            {
             pixelCount = 0;
             state = ro_trailingDummy;
            }
            break;
        case ro_trailingDummy:
            pixelCount++;
            if (pixelCount == trailingDummyElements-1)
            {
             pixelCount = 0;
             state = ro_integrationTime;
             ICG = 0;
            }
            break;
        case ro_integrationTime:
            //wait_us(sensitivity);
            //send_data();
            state = ro_finish;
            break;
        case ro_finish:
            wait_us(sensitivity);
            state = ro_idle;
            ICG = 1;
            LED = 0;
            break;
        case ro_idle:
            break;
        default:
            break;
    }
    shiftGate_int.enable_irq();
}

void print_usage()
{
 //raspi.printf("Usage:\r\n s -\tStart readout\r\n i -\tPrint information about device\r\n b -\tSwitch to binary mode\r\n t -\tSwitch to textual mode\r\n p -\tToggle ping pong communication\r\n h -\tPrint this message\r\n");
 raspi.printf("Usage:\r\n s -\tStart readout\r\n g -\tGet sample\r\n i -\tPrint information about device\r\n h -\tPrint this message\r\n");
}

void print_info()
{
 raspi.printf("Imaging board v%3.1f\r\nADC: %dbit\r\nn of signal elements: %d", SW_VERSION, ADC_BIT_COUNT, signalElements);
 raspi.printf("\r\nSensitivity: %dus\r\nByte ordering: "BYTE_ORDERING"\r\nSize of one pixel: %dB\r\n", sensitivity, sizeof(short));
}

int main()
{
    ICG = 1;
    LED = 0;
    pixelCount = 0;
    state = ro_idle;
    //mode = mode_binary;

    masterClock.period_us(masterFreq_period);
    masterClock.pulsewidth_us(masterFreq_width);

    shiftGate.period_us(shiftGate_period);
    shiftGate.pulsewidth_us(shiftGate_width);

    raspi.baud(BAUDRATE);
    
    //Set parity to odd
    //USART2->CR1 &= 0xFFFFF9FF;
    //USART2->CR1 |= 0x00000600;
    
    //Set two stopbits
    USART2->CR2 &= 0xFFFFCFFF;
    USART2->CR2 |= 0x00002000;
    //enable CTR & RTS
    USART2->CR3 |= 0x00000300;

    shiftGate_int.rise(checkState);
    
    raspi.printf("Ready.\r\n");
    while(1) {
        if (state != ro_idle)   //reading is top priority
        {
         continue;
        }
        
        char c = raspi.getc();
        
        switch (c) {
            case 's':
                LED = 1;
                ICG = 0;
                state = ro_begin;
                break;
            case 'i':
                print_info();
                break;
            /*case 'b':
                mode = mode_binary;
                raspi.printf("Textual mode off.\r\n");
                break;
            case 't':
                mode = mode_textual;
                raspi.printf("Textual mode on.\r\n");
                break;*/
            case 'g':
                LED = 1;
                send_data();
                LED = 0;
                break;
            case 'h':
                print_usage();
                break;
            default:
                raspi.printf("Unexpected character: %c\r\n", c);
                print_usage();
                break;
        }
    }
}