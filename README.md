# I2C Analog/Digital Gamepad for the Raspberry Pi

## Introduction
This project allows you to create a full gamepad using two I2C modules, suitable for use with a variety of games on the Raspberry Pi. The ADS1015 or ADS1115 handle analog input for the joystick, and the MCP23017 handles digital input for up to 16 buttons.

## Prerequisites
Hardware:
- A Raspberry Pi or similar board (the install script was only tested on a Raspberry Pi)
- ADS1015 or ADS1115 I2C module
- MCP23017 I2C module

Software:
- GCC Compiler
- I2C Tools for advanced troubleshooting (sudo apt install -y i2c-tools)

## Hardware Setup
The wiring and setup of your Raspberry Pi for the gamepad is broken down into two parts:

1. Wiring the ADS1015 or ADS1115 Analog-to-Digital Converter (ADC). Both the ADS1015 and ADS1115 use the same I2C communication protocol to read analog values. You can wire each chip to the Pi in the following way:

- ADS1x15 VDD to Raspberry Pi 3.3V
- ADS1x15 GND to Raspberry Pi GND
- ADS1x15 SCL to Raspberry Pi SCL
- ADS1x15 SDA to Raspberry Pi SDA​

2. Wiring the MCP23017 Digital Input. The MCP23017 handles digital input for up to 16 buttons. The wiring of the MCP23017 to the Raspberry Pi is as follows:

- MCP23017 VDD to Raspberry Pi 3.3V
- MCP23017 GND to Raspberry Pi GND
- MCP23017 SCL to Raspberry Pi SCL
- MCP23017 SDA to Raspberry Pi SDA​
- You can change the address of the chip by hooking each of the 3 address pins to either ground or 3.3v. The driver expects all of these pins to be connected to GND, which the module should have done already, meaning that you can leave the pins alone.
- GPB0 – GPB7 and GPA0 – GPA7 are GPIO pins. They will be pulled up to 3.3v by the module, so you should connect each pushbutton to ground.

## Software Setup
After the hardware setup, you can proceed with the software installation.

Installation is done by copying `setup.sh`, `gamepad.c`, `Makefile`, and `gamepad.service` to a folder on your Raspberry Pi (or other Linux device) and running `sudo bash setup.sh`. The driver will compile and I2C will get configured, and then the script will ask whether you want to load the driver at startup.

## Usage Instructions

### Testing the Gamepad

Before you start playing, you might want to test whether the gamepad is working as expected. You can do this by:

- Using a tool like `jstest` or `evtest` to check the status of the joystick and buttons.

### Using the Gamepad in Games

The gamepad should be detected in the same way that USB gamepads are detected. To use the gamepad:

1. Start the game that you want to play.
2. Go to the game settings and select the othermod gamepad as your input device.

### Troubleshooting

If you encounter any issues while using the gamepad, you can try the following:

- Check the wiring of the gamepad, to verify that the MCP23017 AND ADS1x15 I2C modules are correctly connected.

## Contributing

## License


![ADS1015](/images/ads1015.jpg) ![MCP23017](/images/mcp23017.jpg)

[video guide on othermod.com](https://othermod.com/analog-joystick-on-retropie/)
