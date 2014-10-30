#include "mbed.h"
#include "rtos.h"
#include <vector>

//configure I/O
InterruptIn masterClock_int(PB_14);
InterruptIn shiftGate_int(PC_6);
AnalogIn pixelIn(A0);
DigitalOut led(LED1);
DigitalOut illuminator(PA_8);
DigitalOut blueLED(PB_5);
DigitalOut redLED(PB_10);
Serial raspi(USBTX, USBRX);

//testPin indicators - for use with logic analyzer to determine timing states
DigitalOut tp_signalElements(PC_3);
DigitalOut tp_leadingWaste(PC_2);
DigitalOut tp_trailingWaste(PH_1);
DigitalOut tp_t_INT(PH_0);
DigitalOut tp_dump(PC_15);

//define pin configuration for CCD control signals
#define     trigger                 PB_13
#define     shiftGate               PB_8
#define     icg                     PB_3
#define     masterClock             PB_4

//Set up control signals as a bus  (reversed)
BusOut tcd1304dg_clocks(trigger, shiftGate, icg, masterClock);

//define TCD1304AP/DG Characteristics
#define     pixelTotal              3694
#define     leadingDummyElements    16
#define     leadShieldedElements    13
#define     headerElements          3
#define     signalElements          3648
#define     trailingDummyElements   14
//#define     MV(x) ((0xFFF*x)/3300)
                                                //                      MIST  --  masterClock, icg, shiftGate, tigger
#define     transition_1            4           // beginning            0100
#define     transition_2            12          // clock_1              1100
#define     transition_3            4           // just before t2       0100
#define     transition_4            8           // icg fall             1000
#define     transition_5            2           // rise of sh for t3    0010  --  stretch here to increase integration time (must cycle masterClock)
#define     transition_6            10          // t3 masterHigh        1010  --  stretch here to increase integration time (must cycle masterClock)
#define     transition_7            2           // t3 masterLow         0010
#define     transition_8            8           // t1 masterHigh        1000  --  repeat
#define     transition_9            0           // t1 masterLow         0000  --  repeat
#define     transition_10           12          // t4 masterHigh        1100
#define     transition_11           4           // t4 masterLow         0100  --  repeat from here

//define variables
long pixelCount;
int sensitivity                     = 10000;
double pixelData;
float firmwareVersion               = 0.5;
//double pixelValue[signalElements] = { 0 };
uint16_t buffer[pixelTotal] = {0};
uint16_t * const pixelValue = buffer + leadingDummyElements + leadShieldedElements + headerElements;

//Delta encodes a buffer of 16bit integers and sends it through the serial port
//Doesn't save the encoded data anywhere to reduce memory usage
void delta_encode_and_send(const uint16_t *inbuf, int inlen)
{
    uint16_t last = inbuf[0];
    for (int i = 1; i < inlen; i++)
    {
     int16_t diff = last - inbuf[i];    //Calculate the delta
     if (abs(diff) > 127)   //If delta is larger than what we can stuff into one byte, send the whole integer
     {
      raspi.putc(0x80); //Indicate we are sending a whole 16bit integer
      raspi.putc(inbuf[i] >> 8);    //Send the most significant byte
      raspi.putc(inbuf[i] & 0xFF);  //least significant byte
     } else
     {
      raspi.putc(diff & 0xFF);  //If delta fits into one byte, send only the delta
     }
    }
}
 
//Only for demonstration - not used in uC (handled differently in Python code)
bool delta_decode(const char *inbuf, int inlen, std::vector<uint16_t> &out)
{
 if (inlen < 2) //we need at least one value
  return false;
 
 out.clear();
 out.resize(inlen*5/8); //An approximation of the size
 uint16_t last = (inbuf[0] << 8) + inbuf[1];    //read the first value
 out.push_back(last);
 
 for (int i = 2; i < inlen; i++)
 {
  if (inbuf[i] == 0x80) //This byte signifies, that a whole 16bit integer follows
  {
   if (i >= inlen - 2)  //We need two more bytes!
   {    //If there are not enough bytes left in the input buffer, return with failure
    return false;
   }
   last = (inbuf[++i] << 8) + inbuf[++i];   //read the whole integer
   out.push_back(last); //push it to the buffer
  } else    //otherwise we are receiving the delta
  {
   last += inbuf[i];    //calculate the value
   out.push_back(last); //push it to the buffer
  }
 }
 
 return true;   //If we got here, we read all successfuly
}


void printInfo(){
    //time_t seconds = time(NULL);
    //char buffer[32];
    //strftime(buffer, 32, "%I:%M %p\n", localtime(&seconds));
    raspi.printf("meridianScientific\r\n");
    raspi.printf("ramanPi - The DIY 3D Printable RaspberryPi Raman Spectrometer\r\n");
    //raspi.printf("Spectrometer imagingBoard                            %s\r\n", buffer);  //This made the uC crash for some reason
    raspi.printf("-------------------------------------------------------------\r\n");
    raspi.printf("Firmware Version: %f\r\n",firmwareVersion);
    raspi.printf("Current Sensitivity: %d\r\n\r\n\r\n", sensitivity);
}

void dataXfer_thread(void const *args){
    while(1){
        osSignalWait(0x2, osWaitForever);
        /*for (int pixelNumber=0; pixelNumber<signalElements; pixelNumber++)
        {
         //raspi.printf("%i\t%i\r\n", pixelNumber, pixelValue[pixelNumber]);
         //delta_encode_and_send(pixelValue, pixelNumber);
         //raspi.printf("%i\t%4.12f\r\n", pixelNumber, 5 - ((pixelValue[pixelNumber - 1] * 5) / 4096.0));
        }*/
        delta_encode_and_send(pixelValue, signalElements);
        //osDelay(100);
        printf("Done.\r\n");
//        osDelay(5000);
    }
}

void imaging_thread(void const *args){
    while(1){
        int pixelCount = 0;
        osSignalWait(0x1, osWaitForever);//         m  icg  sh  trg
        blueLED = 1;
        tcd1304dg_clocks = transition_1; //         |    |  |   |
        tcd1304dg_clocks = transition_2; //          |   |  |   |
        tcd1304dg_clocks = transition_3; //         |    |  |   |
        tcd1304dg_clocks = transition_4; //          |  |   |   |
        for(int mcInc = 1; mcInc < sensitivity; mcInc++ ){
        tcd1304dg_clocks = transition_5; //         |   |    |  |
        tcd1304dg_clocks = transition_6; //          |  |    |  |
        }
        tcd1304dg_clocks = transition_7; //         |   |    |  |
        for(int mcInc = 1; mcInc < 80; mcInc++ ){        
        tcd1304dg_clocks = transition_8; //          |  |   |   |
        tcd1304dg_clocks = transition_9; //         |   |   |   |
        }
        for(int mcInc = 1; mcInc < pixelTotal; mcInc++ ){        
        tcd1304dg_clocks = transition_10; //         |   |  |   |
        tcd1304dg_clocks = transition_11; //        |    |  |   |
//          FLAG FOR PIXEL READ HERE
            pixelCount++;
            pixelValue[pixelCount] = pixelIn.read_u16();    //This should be balanced with some other command to have 50% duty cycle
                                                            //Later this should also be handled by reading directly from registers to get better timing
            //raspi.printf("%i\t%4.12f\r\n", mcInc, (((pixelIn.read_u16()*5)/65536.0)));
        tcd1304dg_clocks = transition_10; //         |   |  |   |
        tcd1304dg_clocks = transition_11; //        |    |  |   |
        }
        tcd1304dg_clocks = 13;
        blueLED = 0;
    }
}

//define threads for imaging and data transfer
osThreadDef(imaging_thread, osPriorityRealtime, DEFAULT_STACK_SIZE);
osThreadDef(dataXfer_thread, osPriorityNormal, DEFAULT_STACK_SIZE);

void init(){
    tp_signalElements = 0;
    tp_leadingWaste = 0;
    tp_trailingWaste = 0;
    tp_t_INT = 0;
    tp_dump = 0;
    illuminator = 0;
    redLED = 0;
    blueLED = 0;
    pixelCount = 0;
}    

int main(){
    set_time(1256729737); 
    tcd1304dg_clocks = 0;
    init();
    raspi.baud(921600);
    osThreadId img = osThreadCreate(osThread(imaging_thread), NULL);
    osThreadId xfr = osThreadCreate(osThread(dataXfer_thread), NULL);

    raspi.printf("Greetings.\r\n");
    //command interpreter
    while(true){
        char c = raspi.getc();
        switch (c) {
            case 'r':
                osSignalSet(img, 0x1);
                break;
            case 'g':
                osSignalSet(xfr, 0x2);
                break;
            case 'i':
                printInfo();
                break;
            default:
                raspi.putc(c);
                break;
        }
//        osDelay(10);
    }
}