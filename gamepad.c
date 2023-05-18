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

uint8_t ADSreadBuffer[2];
uint8_t ADSwriteBuffer[3];
uint8_t MCP_readBuffer[2];
uint8_t MCPwriteBuffer[2];
uint16_t previousReadBuffer;
uint16_t ADCstore[4] = {
  1650,
  1650,
  1650,
  1650
};

uint8_t numberOfJoysticks = 1;

// specify addresses for expanders
#define MCP_ADDRESS 0x20
#define ADS_ADDRESS 0x48

#define I2C_BUS "/dev/i2c-1" //specify which I2C bus to use

// GPIO expander registers
#define MCP_IODIRA 0x00
#define MCP_GPPUA 0x0C
#define MCP_GPIOA 0x12
#define MCP_IODIRB 0x01
#define MCP_GPPUB 0x0D
#define MCP_GPIOB 0x13

// stuff for the ADC
#define DR1600_DR128 0x80

#define ADS_MODE 1 //single shot mode
#define ADS_INPUT_GAIN 0 //full 6.144v voltage range
#define ADS_COMPARATOR_MODE 0
#define ADS_COMPARATOR_POLARITY 0 //active low
#define ADS_COMPARATOR_LATCH 0
#define ADS_COMPARATOR_QUEUE 0x03 //no comp
#define ADS_OS_ON 0x80 // bit 15
// pointer register
#define ADS_CONVERT_REGISTER 0
#define ADS_CONFIG_REGISTER 1

int MCP_open() {
  // open the i2c device
  int file;
  char * filename = I2C_BUS; //specify which I2C bus to use
  if ((file = open(filename, O_RDWR)) < 0) {
    printf("Failed to open I2C bus %s. Enable it using sudo raspi-config.\n", I2C_BUS);
    exit(1);
  }
  // initialize the device
  if (ioctl(file, I2C_SLAVE, MCP_ADDRESS) < 0) {
    printf("Failed to acquire bus access and/or talk to slave.\n");
    exit(1);
  }
  return file;
}

void MCP_writeConfig(int I2C) {
  // set the pointer to the config register
  MCPwriteBuffer[0] = MCP_IODIRA; // GPIO direction register
  MCPwriteBuffer[1] = 0xFF; // Set GPIO A to input
  write(I2C, MCPwriteBuffer, 2);
  MCPwriteBuffer[0] = MCP_IODIRB; // GPIO direction register
  MCPwriteBuffer[1] = 0xFF; // Set GPIO B to input
  write(I2C, MCPwriteBuffer, 2);
  MCPwriteBuffer[0] = MCP_GPPUA; // GPIO Pullup Register
  MCPwriteBuffer[1] = 0xFF; // Enable Pullup on GPIO A
  write(I2C, MCPwriteBuffer, 2);
  MCPwriteBuffer[0] = MCP_GPPUB; // GPIO Pullup Register
  MCPwriteBuffer[1] = 0xFF; // Enable Pullup on GPIO B
  if (write(I2C, MCPwriteBuffer, 2) != 2) {
    printf("MCP23017 was not detected at address 0x%X. Check wiring and try again.\n", MCP_ADDRESS);
    exit(1);
  }
}

void MCP_read(int I2C) {
  MCPwriteBuffer[0] = MCP_GPIOA;
  write(I2C, MCPwriteBuffer, 1); // prepare to read ports A and B
  //reading two bytes causes it to autoincrement to the next byte, so it reads port B
  if (read(I2C, MCP_readBuffer, 2) != 2) {
    printf("Unable to communicate with the MCP IC\n");
    MCP_readBuffer[0] = 0xFF;
    MCP_readBuffer[1] = 0xFF;
    sleep(1);
  }
}

int ADS_open() {
  // open the i2c device
  int file;
  char * filename = I2C_BUS; //specify which I2C bus to use
  if ((file = open(filename, O_RDWR)) < 0) {
    printf("Failed to open I2C bus %s. Enable it using sudo raspi-config.\n", I2C_BUS);
    exit(1);
  }
  // initialize the device
  if (ioctl(file, I2C_SLAVE, ADS_ADDRESS) < 0) {
    printf("Failed to acquire bus access and/or talk to slave.\n");
    exit(1);
  }
  return file;
}

void ADS_writeConfig(int I2C, int input) { //only needs to be done once
  ADSwriteBuffer[0] = ADS_CONFIG_REGISTER;
  ADSwriteBuffer[1] = ADS_OS_ON + analogInput[input] + ADS_INPUT_GAIN + ADS_MODE;
  ADSwriteBuffer[2] = DR1600_DR128 + ADS_COMPARATOR_MODE + ADS_COMPARATOR_POLARITY + ADS_COMPARATOR_LATCH + ADS_COMPARATOR_QUEUE;
  if (write(I2C, ADSwriteBuffer, 3) != 3) {
    printf("ADS1015/1115 was not detected at address 0x%X. Check wiring and try again, or disable the joystick using argument -j 0.\n", ADS_ADDRESS);
    exit(1);
  }
}

void ADS_setInput(int I2C, int input) { // has to be done every time we read a different input
  ADSwriteBuffer[0] = ADS_CONFIG_REGISTER;
  ADSwriteBuffer[1] = ADS_OS_ON + analogInput[input] + ADS_INPUT_GAIN + ADS_MODE;
  write(I2C, ADSwriteBuffer, 3); // all 3 bytes must be sent each time
  ADSwriteBuffer[0] = ADS_CONVERT_REGISTER; // indicate that we are ready to read the conversion register
  write(I2C, ADSwriteBuffer, 1);
}

void ADS_readInput(int I2C, int input) {
  // read the conversion. we waited long enough for the reading to be ready, so we arent checking the conversion register
  if (read(I2C, ADSreadBuffer, 2) != 2) {
    // if no data was received, center the joystick
    printf("Unable to communicate with the ADS IC\n");
    ADCstore[input] = 1650;
    sleep(1);
  } else {
    ADCstore[input] = (((ADSreadBuffer[0] << 8) | ((ADSreadBuffer[1] & 0xff))) >> 4) * 3; // this should also work with the 1115, and will reduce the 16 bits to 12
  }

}

int createGamepad() {
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
    uidev.absfuzz[ABS_X] = 75; // hysteresis
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    uidev.absmin[ABS_Y] = 0; // center position is 1650
    uidev.absmax[ABS_Y] = 3300; // center position is 1650
    uidev.absflat[ABS_Y] = 75; // deadzone
    uidev.absfuzz[ABS_Y] = 75; // hysteresis
  }

  if (numberOfJoysticks == 2) {
    // right joystick
    ioctl(fd, UI_SET_ABSBIT, ABS_RX);
    uidev.absmin[ABS_RX] = 0; // center position is 1650
    uidev.absmax[ABS_RX] = 3300; // center position is 1650
    uidev.absflat[ABS_RX] = 75; // deadzone
    uidev.absfuzz[ABS_RX] = 75; // hysteresis
    ioctl(fd, UI_SET_ABSBIT, ABS_RY);
    uidev.absmin[ABS_RY] = 0; // center position is 1650
    uidev.absmax[ABS_RY] = 3300; // center position is 1650
    uidev.absflat[ABS_RY] = 75; // deadzone
    uidev.absfuzz[ABS_Y] = 75; // hysteresis
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

  int virtualGamepad = createGamepad(); // create uinput device
  int adcFile;
  if (numberOfJoysticks) {
    adcFile = ADS_open(); // open ADC I2C device
  }
  int mcpFile = MCP_open(); // open Expander device
  //set initial ADC condition
  int ADC = 0;
  int maxADC;
  if (numberOfJoysticks == 2) {
    maxADC = 3;
  } else {
    maxADC = 1;
  }
  if (numberOfJoysticks) {
    ADS_writeConfig(adcFile, ADC);
    ADS_setInput(adcFile, ADC);
  }
  MCP_writeConfig(mcpFile);
  //set initial button condition
  MCP_read(mcpFile);
  uint16_t tempReadBuffer = 0x00;
  updateButtons(virtualGamepad, tempReadBuffer);
  bool reportUinput = 0;
  while (1) {
    if (numberOfJoysticks) {
      ADS_readInput(adcFile, ADC); //read the ADC
      ADC++;
      if (ADC > maxADC) {
        updateJoystick(virtualGamepad); // update the joystick after all analog inputs have been read
        reportUinput = 1;
        ADC = 0;
      }
      ADS_setInput(adcFile, ADC); //set configuration for ADS for next loop
    }
    MCP_read(mcpFile); //read the expander
    tempReadBuffer = (MCP_readBuffer[0] << 8) | (MCP_readBuffer[1] & 0xff);
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
