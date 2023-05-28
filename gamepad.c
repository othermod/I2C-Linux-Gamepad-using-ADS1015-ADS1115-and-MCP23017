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

uint8_t ADS_Buffer[3];
uint8_t MCP_Buffer[2];
uint16_t previousButtons; //why is this defined here? this is just the buttons, right?
uint16_t ADCstore[4] = {
  1650,
  1650,
  1650,
  1650
};

uint8_t numberOfJoysticks = 1;

// specify addresses for expanders
#define MCP_ADDR 0x20
#define ADS_ADDR 0x48

#define I2C_BUS "/dev/i2c-1" //specify which I2C bus to use

// stuff for the activeADC
#define DR1600_DR128 0x80

#define ADS_MODE 1 //single shot mode
#define ADS_INPUT_GAIN 0 //full 6.144v voltage range
#define ADS_COMP_MODE 0
#define ADS_COMP_POLARITY 0 //active low
#define ADS_COMP_LATCH 0
#define ADS_COMP_QUEUE 0x03 //no comp
#define ADS_OS_ON 0x80 // bit 15
// pointer register
#define ADS_CONVERT_REG 0
#define ADS_CONFIG_REG 1

int MCP_select(int file) {
  // initialize the device
  if (ioctl(file, I2C_SLAVE, MCP_ADDR) < 0) {
    printf("Unable to communicate with the MCP23017\n");
    exit(1);
  }
}

void MCP_writeConfig(int file) {
  MCP_Buffer[0] = 0x00; // GPIO direction register A
  MCP_Buffer[1] = 0xFF; // Set GPIO A
  write(file, MCP_Buffer, 2);
  MCP_Buffer[0] = 0x01; // GPIO direction register B
  MCP_Buffer[1] = 0xFF; // Set GPIO B
  write(file, MCP_Buffer, 2);
  MCP_Buffer[0] = 0x0C; // GPIO Pullup Register A
  MCP_Buffer[1] = 0xFF; // Enable Pullup
  write(file, MCP_Buffer, 2);
  MCP_Buffer[0] = 0x0D; // GPIO Pullup Register B
  MCP_Buffer[1] = 0xFF; // Enable Pullup
  if (write(file, MCP_Buffer, 2) != 2) {
    printf("MCP23017 was not detected at address 0x%X. Check wiring and try again.\n", MCP_ADDR);
    exit(1);
  }
}

void MCP_read(int file) {
  MCP_Buffer[0] = 0x12;
  write(file, MCP_Buffer, 1); // prepare to read ports A and B
  //reading two bytes causes it to autoincrement to the next byte, so it reads port B
  if (read(file, MCP_Buffer, 2) != 2) {
    printf("Unable to communicate with the MCP23017\n");
    MCP_Buffer[0] = 0xFF;
    MCP_Buffer[1] = 0xFF;
    sleep(1);
  }
}

int ADS_select(int file) {
  // initialize the device
  if (ioctl(file, I2C_SLAVE, ADS_ADDR) < 0) {
    printf("Unable to communicate with the ADS1x15\n");
    exit(1);
  }
}

void ADS_writeConfig(int file, int input) { //only needs to be done once
  ADS_Buffer[0] = ADS_CONFIG_REG;
  ADS_Buffer[1] = ADS_OS_ON + analogInput[input] + ADS_INPUT_GAIN + ADS_MODE;
  ADS_Buffer[2] = DR1600_DR128 + ADS_COMP_MODE + ADS_COMP_POLARITY + ADS_COMP_LATCH + ADS_COMP_QUEUE;
  if (write(file, ADS_Buffer, 3) != 3) {
    printf("ADS1x15 was not detected at address 0x%X. Check wiring and try again, or disable the joystick using argument -j 0.\n", ADS_ADDR);
    exit(1);
  }
}

void ADS_setInput(int file, int input) { // has to be done every time we read a different input
  ADS_Buffer[0] = ADS_CONFIG_REG;
  ADS_Buffer[1] = ADS_OS_ON + analogInput[input] + ADS_INPUT_GAIN + ADS_MODE;
  ADS_Buffer[2] = DR1600_DR128 + ADS_COMP_MODE + ADS_COMP_POLARITY + ADS_COMP_LATCH + ADS_COMP_QUEUE;
  write(file, ADS_Buffer, 3); // all 3 bytes must be sent each time
  ADS_Buffer[0] = ADS_CONVERT_REG; // indicate that we are ready to read the conversion register
  write(file, ADS_Buffer, 1);
}

void ADS_readInput(int file, int input) {
  // read the conversion. we waited long enough for the reading to be ready, so we arent checking the conversion register
  if (read(file, ADS_Buffer, 2) != 2) {
    // if no data was received, center the joystick
    printf("Unable to communicate with the ADS1x15\n");
    ADCstore[input] = 1650;
    sleep(1);
  } else {
    ADCstore[input] = (((ADS_Buffer[0] << 8) | ((ADS_Buffer[1] & 0xff))) >> 4) * 3; // this should also work with the 1115, and will reduce the 16 bits to 12
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
  for (int i = BTN_TRIGGER_HAPPY1; i <= BTN_TRIGGER_HAPPY16; i++) {
    ioctl(fd, UI_SET_KEYBIT, i);
  }

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
  for (int i = 0; i < 16; i++) {
    emit(virtualGamepad, EV_KEY, BTN_TRIGGER_HAPPY1 + i, !(buttons >> i & 1));
  }
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

  // create uinput device
  int virtualGamepad = createGamepad();

  // open the i2c bus
  int file;
  if ((file = open(I2C_BUS, O_RDWR)) < 0) {
      perror("Failed to open the bus.");
      return EXIT_FAILURE;
    }

  //set initial condition
  int activeADC = 0;
  if (numberOfJoysticks) {
    ADS_select(file);
    ADS_writeConfig(file, activeADC);
    ADS_setInput(file, activeADC);
  }
  MCP_select(file);
  MCP_writeConfig(file);
  //set initial button condition
  MCP_read(file);
  uint16_t combinedButtons = 0x00;
  updateButtons(virtualGamepad, combinedButtons);
  bool reportUinput = 0;
  while (1) {
    if (numberOfJoysticks) {
      ADS_select(file);
      ADS_readInput(file, activeADC); //read the ADC
      activeADC++;
      if (activeADC > (numberOfJoysticks * 2 - 1)) {
        updateJoystick(virtualGamepad); // update the joystick after all analog inputs have been read
        reportUinput = 1;
        activeADC = 0;
      }
      ADS_setInput(file, activeADC); //set configuration for ADS for next loop
    }
    MCP_select(file);
    MCP_read(file); //read the expander
    combinedButtons = (MCP_Buffer[0] << 8) | (MCP_Buffer[1] & 0xff);
    if (combinedButtons != previousButtons) {
      updateButtons(virtualGamepad, combinedButtons);
      reportUinput = 1;
    } //only update the buttons when something changes from the last loop
    previousButtons = combinedButtons;

    if (reportUinput) {
      emit(virtualGamepad, EV_SYN, SYN_REPORT, 0);
      reportUinput = 0;
    }

    usleep(16666); // sleep for about 1/60th of a second. Also gives the ADC enough time to prepare the next reading
  }
  return 0;
}
