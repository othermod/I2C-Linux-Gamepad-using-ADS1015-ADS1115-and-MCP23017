#include "string.h"
#include "pthread.h"
#include "stdio.h"
#include "errno.h"
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdbool.h>

const int analogInput[] = {
  0x40,
  0x50,
  0x60,
  0x70
}; //Analog Selection for 0/1/2/3

uint8_t readBuffer[2];
uint8_t writeBuffer[3];
uint16_t ADCstore[4];
int verbose = 0;
//this was done on every loop, which isnt needed

#define DR128 0
#define DR250 0x20
#define DR490 0x40
#define DR920 0x60
#define DR1600 0x80
#define DR2400 0xA0
#define DR3300 0xC0

#define ADS1015_MODE 1 //single shot mode
#define ADS1015_INPUT_GAIN 0 //full 6.144v voltage range
#define ADS1015_COMPARATOR_MODE 0
#define ADS1015_COMPARATOR_POLARITY 0 //active low
#define ADS1015_COMPARATOR_LATCH 0
#define ADS1015_COMPARATOR_QUEUE 0x03 //no comp
#define ADS1015_OS_ON 0x80 // bit 15
// pointer register
#define ADS1015_CONVERT_REGISTER 0
#define ADS1015_CONFIG_REGISTER 1

int ads1015_open() {
  // open the i2c device
  int file;
  const int ADC_Address = 0x48; //0x48 is the i2c address
  char * filename = "/dev/i2c-1"; //specify which I2C bus to use
  if ((file = open(filename, O_RDWR)) < 0) {
    perror("Failed to open the i2c bus");
    exit(1);
  }
  // initialize the device
  if (ioctl(file, I2C_SLAVE, ADC_Address) < 0) //0x48 is the i2c address
  {
    printf("Failed to acquire bus access and/or talk to slave.\n");
    exit(1);
  }
  return file;
}

void ads1015SetConfig(int I2C, int input) { //only needs to be done when changing the input
  writeBuffer[1] = ADS1015_OS_ON + analogInput[0] + ADS1015_INPUT_GAIN + ADS1015_MODE;
  if (verbose == 1) printf("writeBuffer[1] : %#x\n", writeBuffer[1]);
  writeBuffer[2] = DR128 + ADS1015_COMPARATOR_MODE + ADS1015_COMPARATOR_POLARITY + ADS1015_COMPARATOR_LATCH + ADS1015_COMPARATOR_QUEUE;
  if (verbose == 1) printf("Setting Configs. writeBuffer[2] : %#x\n", writeBuffer[2]);
  writeBuffer[0] = ADS1015_CONFIG_REGISTER;
  writeBuffer[1] = ADS1015_OS_ON + analogInput[input] + ADS1015_INPUT_GAIN + ADS1015_MODE;
  write(I2C, writeBuffer, 3);
  writeBuffer[0] = ADS1015_CONVERT_REGISTER; // indicate that we are ready to read the conversion register
  write(I2C, writeBuffer, 1);
}

void ads1015WriteConfig(int I2C, int input) {
  // set the pointer to the config register
  writeBuffer[0] = ADS1015_CONFIG_REGISTER;
  writeBuffer[1] = ADS1015_OS_ON + analogInput[input] + ADS1015_INPUT_GAIN + ADS1015_MODE;
  write(I2C, writeBuffer, 3);
  writeBuffer[0] = ADS1015_CONVERT_REGISTER; // indicate that we are ready to read the conversion register
  write(I2C, writeBuffer, 1);
}

void readADC(int I2C, int input) {
  read(I2C, readBuffer, 2); // read the conversion. we waited long enough for the reading to be ready, so we arent checking the conversion register
  //int val = readBuffer[0] << 8 | readBuffer[1];
  ADCstore[input] = ((readBuffer[0] << 8) | ((readBuffer[1] & 0xff)));
  ADCstore[input] = (ADCstore[input] >> 4) * 3; //bitshift to the right 4 places (see datasheet for reason, and multiply by 3 to get actual voltage)
  //return val;
  if(verbose) {printf("ADC%d: %d\n", input, ADCstore[input]);}
}

int createUInputDevice() {
  int fd;
  fd = open("/dev/uinput", O_WRONLY | O_NDELAY);
  if (fd < 0) {
    fprintf(stderr, "Can't open uinput device!\n");
    exit(1);
  }
  // device structure
  struct uinput_user_dev uidev;
  memset( & uidev, 0, sizeof(uidev));
  // init event
  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_EVBIT, EV_REL);
  // button

  // axis
  ioctl(fd, UI_SET_EVBIT, EV_ABS);
  ioctl(fd, UI_SET_ABSBIT, ABS_X);
  uidev.absmin[ABS_X] = 0; //center position is 127, minimum is near 50
  uidev.absmax[ABS_X] = 3300; //center position is 127, maximum is near 200
  uidev.absflat[ABS_X] = 50; //this appears to be the deadzone
  //uidev.absfuzz[ABS_X] = 0; //what does this do?
  ioctl(fd, UI_SET_ABSBIT, ABS_Y);
  uidev.absmin[ABS_Y] = 0; //center position is 127, minimum is near 50
  uidev.absmax[ABS_Y] = 3300; //center position is 127, maximum is near 200
  uidev.absflat[ABS_Y] = 50; //this appears to be the deadzone
  //uidev.absfuzz[ABS_Y] = 0; //what does this do?
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "PSPi Controller");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 1;
  uidev.id.product = 5;
  uidev.id.version = 1;
  write(fd, & uidev, sizeof(uidev));
  if (ioctl(fd, UI_DEV_CREATE)) {
    fprintf(stderr, "Error while creating uinput device!\n");
    exit(1);
  }
  return fd;
}

void sendInputEvent(int fd, uint16_t type, uint16_t code, int32_t value) {
  struct input_event ev;
  memset( & ev, 0, sizeof(ev));
  ev.type = type;
  ev.code = code;
  ev.value = value;
  if (write(fd, & ev, sizeof(ev)) < 0) {
    fprintf(stderr, "1Error while sending event to uinput device!\n");
  }
  // need to send a sync event
  ev.type = EV_SYN;
  ev.code = SYN_REPORT;
  ev.value = 0;
  write(fd, & ev, sizeof(ev));
  if (write(fd, & ev, sizeof(ev)) < 0) {
    fprintf(stderr, "2Error while sending event to uinput device!\n");
  }
}

void updateButtons(int UInputFIle) {
  //disabling joystick for now, will finish that later
  sendInputEvent(UInputFIle, EV_ABS, ABS_X, ADCstore[0]);
  sendInputEvent(UInputFIle, EV_ABS, ABS_Y, ADCstore[1]);
}

int main(void) {
  int adcFile = ads1015_open(); // open ADC I2C device

  //set initial ADC condition
  int ADC = 0;
  ads1015SetConfig(adcFile, ADC);
  usleep(15000);
  //comment out until everything else is done
  int UInputFIle = createUInputDevice(); // create uinput device
  //set initial button condition
  uint16_t tempReadBuffer = 0x00;
  while (1) {
		readADC(adcFile, ADC); //read the ADC
    ADC = !ADC;
		ads1015WriteConfig(adcFile, ADC); //set configuration for ADS1015 for next loop
    usleep(15000); // sleep for about 1/60th of a second. Also gives the ADC enough time to prepare the next reading
    readADC(adcFile, ADC); //read the ADC
    ADC = !ADC;
    ads1015WriteConfig(adcFile, ADC); //set configuration for ADS1015 for next loop
    updateButtons(UInputFIle);  //only update the controller when a button is pressed for the time being. will add a check for joystick later
    usleep(15000); // sleep for about 1/60th of a second. Also gives the ADC enough time to prepare the next reading
  }
  return 0;
}
