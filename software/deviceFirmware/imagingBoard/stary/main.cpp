#include "mbed.h"

#include <vector>
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

unsigned short pixelValue[signalElements];
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

//sends the buffered data over serial port in lines as text
void send_data_textual()
{
 std::vector<int> resend;
 //bool all_sent = false;
 int tmp;
 char ch;
 
 //wait for sync
 while ((ch = raspi.getc()) != 'c')
 {
  if (ch == 'm')
   return;
  raspi.putc('r');
 }
 
 for (int i = 0; i < signalElements; i++)
 {
  raspi.printf("%d:%d:%d\r\n", i, pixelValue[i], i+pixelValue[i]);
 }
 
 while (true)
 { 
  if (raspi.getc() == 'r')
  {
   do
   {
    raspi.scanf("%i", &tmp);
    resend.push_back(tmp);
   } while (raspi.getc() == 'r');
  } else
  {
   break;
  }
  
  for (std::vector<int>::iterator it = resend.begin() ; it != resend.end(); ++it)
   raspi.printf("%d:%d:%d\r\n", (*it), pixelValue[(*it)], (*it)+pixelValue[(*it)]);
  
  resend.clear();
 }
}

//sends the buffered data over serial port in chunks as bytes
void send_data_binary()
{
 bool all_sent = false;
 unsigned char chunk_n;
 char ch;
 int chunk_len = 522;
 int total_len = 7296;
 int to_send = 0;
 bool ok;
 int checksum;
 
 //shiftGate_int.disable_irq();
 
 while ((ch = raspi.getc()) != 'c')
 {
  if (ch == 'm')
   return;
  raspi.putc('r');
 }
 
 
 while (!all_sent)
 {
     //oldsent = to_send;
     ok = true;
     chunk_n = raspi.getc();
     raspi.putc(chunk_n);
     
     if (chunk_n == total_len/chunk_len)
         to_send = total_len;
     else
         to_send += chunk_len;
     
     all_sent = false;
     
     //while (!all_sent)
     {
      wait_ms(2);
      checksum = 0;
      
      
      //raspi.putc(chunk_n);
      for (int byte_n = chunk_len*chunk_n; byte_n < to_send; byte_n++)
      {
       checksum += b_pixelValue[byte_n];
       raspi.putc(b_pixelValue[byte_n]);
       if (raspi.readable())
       {
        ok = false;
        break;
       }
      }
      
      if (ok)
      {
       checksum += 1;
       raspi.putc(checksum%256);
      }
      
      ch = raspi.getc();
      if (ch == 'm')
      {
       return;
      } else if (ch != 'c')
      {
       while ((ch = raspi.getc()) != 'c')
       {
        if (ch == 'm')
         return;
        raspi.putc('r');
       }
       
       while (raspi.readable())
        raspi.getc();
      }
     }
 }
 
 //shiftGate_int.enable_irq();
 
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
 raspi.printf("Imaging board v%3.1f\r\nADC: %dbit\r\nn of signal elements: %d\r\nCommunication mode: ", SW_VERSION, ADC_BIT_COUNT, signalElements);
 if (mode == mode_binary)
  raspi.printf("binary");
 else
  raspi.printf("textual");
 raspi.printf("\r\nSensitivity: %dus\r\nByte ordering: "BYTE_ORDERING"\r\nSize of one pixel: %dB\r\n", sensitivity, sizeof(short));
}

int main()
{
    ICG = 1;
    LED = 0;
    pixelCount = 0;
    state = ro_idle;
    mode = mode_binary;

    masterClock.period_us(masterFreq_period);
    masterClock.pulsewidth_us(masterFreq_width);

    shiftGate.period_us(shiftGate_period);
    shiftGate.pulsewidth_us(shiftGate_width);

    raspi.baud(BAUDRATE);
    
    //Set parity to odd - produces garbage for some reason
    //USART2->CR1 &= 0xFFFFF9FF;
    //USART2->CR1 |= 0x00000600;
    
    //Set two stopbits
    USART2->CR2 &= 0xFFFFCFFF;
    USART2->CR2 |= 0x00002000;
    //enable CTS & RTS
    USART2->CR3 |= 0x00000300;

    shiftGate_int.rise(checkState);
    
    raspi.printf("Ready.\r\n");
    while(1) {
        if (state != ro_idle)   //reading is top priority
        {
         continue;
        }
        
        raspi.printf("Menu.\r\n");
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
            case 'g':
                LED = 1;
                raspi.printf("Sending data.\r\n");
                if (mode == mode_binary)
                {
                 send_data_binary();
                } else
                {
                 send_data_textual();
                }
                LED = 0;
                break;
            case 'b':
                mode = mode_binary;
                break;
            case 't':
                mode = mode_textual;
                break;
            case 'h':
                print_usage();
                break;
            case 'm':
                break;
            default:
                raspi.printf("Unexpected character: %c\r\n", c);
                print_usage();
                break;
        }
    }
}