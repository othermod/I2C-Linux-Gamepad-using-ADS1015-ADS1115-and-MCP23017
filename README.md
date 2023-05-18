# Analog/Digital Gamepad on the Raspberry Pi

The driver is written in C.

This code allows you to create a full gamepad using two I2C modules. The ADS1015 or ADS1115 handle analog input for the joystick, and the MCP23017 handles digital input for up to 16 buttons.

Installation is done by copying setup.sh, gamepad.c, Makefile, and gamepad.service to a folder on your Raspberry Pi (or other linux device) and running ```sudo bash setup.sh```. The driver will compile and I2C will get configured, and then the script will ask whether you want to load the driver at startup.

![ADS1015](/images/ads1015.jpg) ![MCP23017](/images/mcp23017.jpg)

[guide on othermod.com](https://othermod.com/analog-joystick-on-retropie/)
