#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdbool.h>

#define MCP_ADDRESS 0x20
#define ADS_ADDRESS 0x48

bool checkDeviceOnBus(const char* bus, int addr) {
    int file;
    char buf[1] = {0};

    if ((file = open(bus, O_RDWR)) < 0) {
        //printf("Failed to open bus %s.\n", bus);
        return 0;
    }

    if (ioctl(file, I2C_SLAVE, addr) < 0) {
        //printf("Failed to acquire bus access or talk to slave.\n");
        close(file);
        return 0;
    }

    // Try to read a byte from the device
    if (read(file, buf, 1) < 0) {
        //printf("Nothing found on %s.\n", bus);
        return 0;
    } else {
        //printf("Device was found at address 0x%x on %s.\n", addr, bus);
        return 1;
    }
    close(file);
}

int main() {
  bool foundMCP;
  bool foundADS;
  bool foundAnything = 0;
    for (int i = 0; i < 23; i++) { // typically there are 2 buses: /dev/i2c-0 and /dev/i2c-1
        char filename[20];
        sprintf(filename, "/dev/i2c-%d", i);
        foundMCP = checkDeviceOnBus(filename, MCP_ADDRESS);
        foundADS = checkDeviceOnBus(filename, ADS_ADDRESS);
        if (foundMCP & foundADS) {
          printf("Found both the MCP23017 and ADS1x15 on i2c-%d\n", i);
          foundAnything = 1;
        } else if (foundMCP & !foundADS){
          printf("Found only the MCP23017 on i2c-%d\n", i);
          foundAnything = 1;
        } else if (!foundMCP & foundADS){
          printf("Found only the ADS1x15 on i2c-%d\n", i);
          foundAnything = 1;
        }
    }
    if (!foundAnything) {
      printf("Found neither module during scanning. Check your wiring.\n");
    }
    return 0;
}
