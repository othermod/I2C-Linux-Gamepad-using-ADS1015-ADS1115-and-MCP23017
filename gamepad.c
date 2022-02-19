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
}; // Analog Selection for 0/1/2/3

uint8_t readBuffer[2];
uint8_t writeBuffer[3];

uint8_t ADS1015readBuffer[2];
uint8_t ADS1015writeBuffer[3];
uint8_t MCP23017readBuffer[2];
uint8_t MCP23017writeBuffer[2];
uint16_t previousReadBuffer;
uint16_t ADCstore[4] = {
  1650,
  1650,
  1650,
  1650
};

uint8_t numberOfJoysticks = 1;

// specify addresses for expanders
#define MCP23017_ADDRESS 0x20
#define ADS1015_ADDRESS 0x48

#define I2C_BUS "/dev/i2c-1" //specify which I2C bus to use

// stuff for the GPIO expander
#define MCP23017_IODIRA 0x00
#define MCP23017_IPOLA 0x02
#define MCP23017_GPINTENA 0x04
#define MCP23017_DEFVALA 0x06
#define MCP23017_INTCONA 0x08
#define MCP23017_IOCONA 0x0A
#define MCP23017_GPPUA 0x0C
#define MCP23017_INTFA 0x0E
#define MCP23017_INTCAPA 0x10
#define MCP23017_GPIOA 0x12
#define MCP23017_OLATA 0x14

#define MCP23017_IODIRB 0x01
#define MCP23017_IPOLB 0x03
#define MCP23017_GPINTENB 0x05
#define MCP23017_DEFVALB 0x07
#define MCP23017_INTCONB 0x09
#define MCP23017_IOCONB 0x0B
#define MCP23017_GPPUB 0x0D
#define MCP23017_INTFB 0x0F
#define MCP23017_INTCAPB 0x11
#define MCP23017_GPIOB 0x13
#define MCP23017_OLATB 0x15

// stuff for the ADC
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

int MCP23017open() {
  // open the i2c device
  int file;
  char * filename = I2C_BUS; //specify which I2C bus to use
  if ((file = open(filename, O_RDWR)) < 0) {
    printf("Failed to open I2C bus %s. Enable it using sudo raspi-config.\n", I2C_BUS);
    exit(1);
  }
  // initialize the device
  if (ioctl(file, I2C_SLAVE, MCP23017_ADDRESS) < 0) {
    printf("Failed to acquire bus access and/or talk to slave.\n");
    exit(1);
  }
  return file;
}

void MCP23017writeConfig(int I2C) {
  // set the pointer to the config register
  MCP23017writeBuffer[0] = MCP23017_IODIRA; // GPIO direction register
  MCP23017writeBuffer[1] = 0xFF; // Set GPIO A to input
  write(I2C, MCP23017writeBuffer, 2);
  MCP23017writeBuffer[0] = MCP23017_IODIRB; // GPIO direction register
  MCP23017writeBuffer[1] = 0xFF; // Set GPIO B to input
  write(I2C, MCP23017writeBuffer, 2);
  MCP23017writeBuffer[0] = MCP23017_GPPUA; // GPIO Pullup Register
  MCP23017writeBuffer[1] = 0xFF; // Enable Pullup on GPIO A
  write(I2C, MCP23017writeBuffer, 2);
  MCP23017writeBuffer[0] = MCP23017_GPPUB; // GPIO Pullup Register
  MCP23017writeBuffer[1] = 0xFF; // Enable Pullup on GPIO B
  if (write(I2C, MCP23017writeBuffer, 2) != 2) {
    printf("MCP23017 was not detected at address 0x%X. Buttons will not function.\n", MCP23017_ADDRESS);
  }
}

void MCP23017read(int I2C) {
  MCP23017writeBuffer[0] = MCP23017_GPIOA;
  write(I2C, MCP23017writeBuffer, 1); // prepare to read ports A and B
  //reading two bytes causes it to autoincrement to the next byte, so it reads port B
  if (read(I2C, MCP23017readBuffer, 2) != 2) {
    printf("Unable to communicate with the MCP23017\n");
    MCP23017readBuffer[0] = 0xFF;
    MCP23017readBuffer[1] = 0xFF;
    sleep(1);
  }
}

int ADS1015open() {
  // open the i2c device
  int file;
  char * filename = I2C_BUS; //specify which I2C bus to use
  if ((file = open(filename, O_RDWR)) < 0) {
    printf("Failed to open I2C bus %s. Enable it using sudo raspi-config.\n", I2C_BUS);
    exit(1);
  }
  // initialize the device
  if (ioctl(file, I2C_SLAVE, ADS1015_ADDRESS) < 0) {
    printf("Failed to acquire bus access and/or talk to slave.\n");
    exit(1);
  }
  return file;
}

void ADS1015writeConfig(int I2C, int input) { //only needs to be done once
  ADS1015writeBuffer[0] = ADS1015_CONFIG_REGISTER;
  ADS1015writeBuffer[1] = ADS1015_OS_ON + analogInput[input] + ADS1015_INPUT_GAIN + ADS1015_MODE;
  ADS1015writeBuffer[2] = DR490 + ADS1015_COMPARATOR_MODE + ADS1015_COMPARATOR_POLARITY + ADS1015_COMPARATOR_LATCH + ADS1015_COMPARATOR_QUEUE;
  if (write(I2C, ADS1015writeBuffer, 3) != 3) {
    printf("ADS1015 was not detected at address 0x%X. Joystick will not function.\n", ADS1015_ADDRESS);
  }
}

void ADS1015setInput(int I2C, int input) { // has to be done every time we read a different input
  ADS1015writeBuffer[0] = ADS1015_CONFIG_REGISTER;
  ADS1015writeBuffer[1] = ADS1015_OS_ON + analogInput[input] + ADS1015_INPUT_GAIN + ADS1015_MODE;
  write(I2C, ADS1015writeBuffer, 3); // all 3 bytes must be sent each time
  ADS1015writeBuffer[0] = ADS1015_CONVERT_REGISTER; // indicate that we are ready to read the conversion register
  write(I2C, ADS1015writeBuffer, 1);
}

void ADS1015readInput(int I2C, int input) {
  // read the conversion. we waited long enough for the reading to be ready, so we arent checking the conversion register
  if (read(I2C, ADS1015readBuffer, 2) != 2) {
    // if no data was received, center the joystick
    printf("Unable to communicate with the ADS1015\n");
    ADCstore[input] = 1650;
    sleep(1);
  } else {
    ADCstore[input] = (((ADS1015readBuffer[0] << 8) | ((ADS1015readBuffer[1] & 0xff))) >> 4) * 3;
  }

}

int createUInputDevice() {
  int fd;
  fd = open("/dev/uinput", O_WRONLY | O_NDELAY);
  if (fd < 0) {
    fprintf(stderr, "Unable to create gamepad with uinput. Try running as sudo.\n");
    exit(1);
  }
  // device structure
  struct uinput_user_dev uidev;
  memset( & uidev, 0, sizeof(uidev));
  // init event
  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_EVBIT, EV_REL);
  // button
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY1);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY2);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY3);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY4);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY5);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY6);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY7);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY8);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY9);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY10);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY11);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY12);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY13);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY14);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY15);
  ioctl(fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY16);

  // axis
  if (numberOfJoysticks) {
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    // left joystick
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    uidev.absmin[ABS_X] = 0; // center position is 1650
    uidev.absmax[ABS_X] = 3300; // center position is 1650
    uidev.absflat[ABS_X] = 75; // deadzone
    //uidev.absfuzz[ABS_X] = 0; // what does this do?
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    uidev.absmin[ABS_Y] = 0; // center position is 1650
    uidev.absmax[ABS_Y] = 3300; // center position is 1650
    uidev.absflat[ABS_Y] = 75; // deadzone
    //uidev.absfuzz[ABS_Y] = 0; // what does this do?
  }

  if (numberOfJoysticks == 2) {
    // right joystick
    ioctl(fd, UI_SET_ABSBIT, ABS_RX);
    uidev.absmin[ABS_RX] = 0; // center position is 1650
    uidev.absmax[ABS_RX] = 3300; // center position is 1650
    uidev.absflat[ABS_RX] = 75; // deadzone
    //uidev.absfuzz[ABS_RX] = 0; // what does this do?
    ioctl(fd, UI_SET_ABSBIT, ABS_RY);
    uidev.absmin[ABS_RY] = 0; // center position is 1650
    uidev.absmax[ABS_RY] = 3300; // center position is 1650
    uidev.absflat[ABS_RY] = 75; // deadzone
    //uidev.absfuzz[ABS_Y] = 0; // what does this do?
  }

  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "othermod Gamepad");
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

void emit(int virtualGamepad, int type, int code, int val) {
  struct input_event ie;
  ie.type = type;
  ie.code = code;
  ie.value = val;
  /* timestamp values below are ignored */
  ie.time.tv_sec = 0;
  ie.time.tv_usec = 0;
  write(virtualGamepad, & ie, sizeof(ie));
}

void updateButtons(int virtualGamepad, int buttons) {
  // update button event
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY1, !((buttons >> 0x00) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY2, !((buttons >> 0x01) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY3, !((buttons >> 0x02) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY4, !((buttons >> 0x03) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY5, !((buttons >> 0x04) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY6, !((buttons >> 0x05) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY7, !((buttons >> 0x06) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY8, !((buttons >> 0x07) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY9, !((buttons >> 0x08) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY10, !((buttons >> 0x09) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY11, !((buttons >> 0x0A) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY12, !((buttons >> 0x0B) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY13, !((buttons >> 0x0C) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY14, !((buttons >> 0x0D) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY15, !((buttons >> 0x0E) & 1));
  emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY16, !((buttons >> 0x0F) & 1));
}

void updateJoystick(int virtualGamepad) {
  // update joystick
  emit(virtualGamepad, EV_ABS, ABS_X, ADCstore[0]);
  emit(virtualGamepad, EV_ABS, ABS_Y, ADCstore[1]);
  if (numberOfJoysticks == 2) {
    emit(virtualGamepad, EV_ABS, ABS_RX, ADCstore[2]);
    emit(virtualGamepad, EV_ABS, ABS_RY, ADCstore[3]);
  }
}


int main(int argc, char * argv[]) {
  int ctr;
     for( ctr=0; ctr < argc; ctr++ ) {
        if ((!strcmp("-j", argv[ctr])) || (!strcmp("--joysticks", argv[ctr]))) {
          if (argc < (ctr + 2)) {
            printf("Must include the number of joysticks (0, 1 or 2)\n");
            exit(1);
          }
          if (!strcmp("0", argv[ctr + 1])) {
            numberOfJoysticks = 0;
          } else if (!strcmp("1", argv[ctr + 1])) {
            numberOfJoysticks = 1;
          } else if (!strcmp("2", argv[ctr + 1])) {
            numberOfJoysticks = 2;
          } else {
            printf("Incorrect number of joysticks requested (must be 0, 1, or 2)\n");
            exit(1);
          }
        }
     }

  int virtualGamepad = createUInputDevice(); // create uinput device
  int adcFile;
  if (numberOfJoysticks) {
    adcFile = ADS1015open(); // open ADC I2C device
  }
  int mcpFile = MCP23017open(); // open Expander device
  //set initial ADC condition
  int ADC = 0;
  int maxADC;
  if (numberOfJoysticks == 2) {
    maxADC = 3;
  } else {
    maxADC = 1;
  }
  if (numberOfJoysticks) {
    ADS1015writeConfig(adcFile, ADC);
    ADS1015setInput(adcFile, ADC);
  }
  MCP23017writeConfig(mcpFile);
  //set initial button condition
  MCP23017read(mcpFile);
  uint16_t tempReadBuffer = 0x00;
  updateButtons(virtualGamepad, tempReadBuffer);
  bool reportUinput = 0;
  while (1) {
    if (numberOfJoysticks) {
      ADS1015readInput(adcFile, ADC); //read the ADC
      ADC++;
      if (ADC > maxADC) {
        updateJoystick(virtualGamepad); // update the joystick after all analog inputs have been read
        reportUinput = 1;
        ADC = 0;
      }
      ADS1015setInput(adcFile, ADC); //set configuration for ADS1015 for next loop
    }
    MCP23017read(mcpFile); //read the expander
    tempReadBuffer = (MCP23017readBuffer[0] << 8) | (MCP23017readBuffer[1] & 0xff);
    if (tempReadBuffer != previousReadBuffer) {
      updateButtons(virtualGamepad, tempReadBuffer);
      reportUinput = 1;
    } //only update the buttons when something changes from the last loop
    previousReadBuffer = tempReadBuffer;

    if (reportUinput) {
      emit(virtualGamepad, EV_SYN, SYN_REPORT, 0);
      reportUinput = 0;
    }

    usleep(16666); // sleep for about 1/60th of a second. Also gives the ADC enough time to prepare the next reading
  }
  return 0;
}
