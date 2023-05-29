# I2C Analog/Digital Gamepad for the Raspberry Pi

## Overview

This project allows you to create a gamepad using two I2C modules, suitable for a variety of games on the Raspberry Pi. The gamepad created can handle both analog and digital input, giving you a full-fledged gaming experience. This project is perfect for gaming enthusiasts looking for a fun and engaging DIY project, educators teaching technology or programming, and anyone interested in learning more about the interaction between hardware and software in game controllers.

## Introduction

The ADS1015 or ADS1115 handle analog input for the joystick, and the MCP23017 handles digital input for up to 16 buttons. By the end of this guide, you'll have a functional gamepad that you can use with your favorite games on your Raspberry Pi.

## Prerequisites
Hardware:
- A Raspberry Pi or similar board (the install script was only tested on a Raspberry Pi)
- ADS1015 or ADS1115 I2C module
- MCP23017 I2C module

Software:
- GCC Compiler
- Jstest to verify function
- I2C Tools for advanced troubleshooting

## Hardware Setup

![diagram](/images/diagram.png)

The wiring and setup of your Raspberry Pi for the gamepad is broken down into two parts:

1. Wiring the ADS1015 or ADS1115 Analog-to-Digital Converter (ADC). Both the ADS1015 and ADS1115 use the same I2C communication protocol to read analog values. You can wire each chip to the Pi in the following way:

- ADS1x15 VDD to Raspberry Pi 3.3V
- ADS1x15 GND to Raspberry Pi GND
- ADS1x15 SCL to Raspberry Pi SCL
- ADS1x15 SDA to Raspberry Pi SDA​
- ADS1x15 A0 to the X axis of your left joystick
- ADS1x15 A1 to the Y axis of your left joystick
- ADS1x15 A2 to the X axis of your right joystick
- ADS1x15 A3 to the Y axis of your right joystick
- Joystick VDD to Raspberry Pi 3.3V
- Joystick GND to Raspberry Pi GND

2. Wiring the MCP23017 Digital Input. The MCP23017 handles digital input for up to 16 buttons. The wiring of the MCP23017 to the Raspberry Pi is as follows:

- MCP23017 VDD to Raspberry Pi 3.3V
- MCP23017 GND to Raspberry Pi GND
- MCP23017 SCL to Raspberry Pi SCL
- MCP23017 SDA to Raspberry Pi SDA​
- You can change the address of the chip by hooking each of the 3 address pins to either ground or 3.3v. The driver expects all of these pins to be connected to GND, which the module should have done already, meaning that you can leave the pins alone.
- GPB0 – GPB7 and GPA0 – GPA7 to your pushbuttons. The GPIOs are pulled up to 3.3v by the module, so you should connect each pushbutton to ground.

## Software Setup
Before proceeding with the software installation, ensure that you have the necessary software installed on your Raspberry Pi, including the GCC Compiler.

If you haven't installed GCC Compiler, you can do so with the following command:
```
sudo apt-get install -y gcc
```

Installation is done by copying `setup.sh`, `gamepad.c`, `scan.c`, and `gamepad.service` to a folder on your Raspberry Pi (or other Linux device) and running `sudo bash setup.sh`. The driver will compile and I2C will get configured, and then the script will ask whether you want to load the driver at startup.

## Usage Instructions

### Testing the Gamepad

Before you start playing, you might want to test whether the gamepad is working as expected. You can do this using a tool like jstest.

To use jstest, you can use the following command:
```
jstest /dev/input/js0
```

If jstest is not installed, you can install it with:
```
sudo apt-get install -y joystick
```

The command will display the status of your joystick and buttons. It assumes that the othermod gamepad is the first gamepad. If your device is not at js0, you may need to try other numbers such as js1, etc.

You can list your devices with the following command:
```
ls /dev/input/js*
```

### Using the Gamepad in Games

The gamepad should be detected in the same way that USB gamepads are detected. To use the gamepad, go to the game settings and select the othermod gamepad as your input device.

### Troubleshooting

If you encounter any issues while using the gamepad, verify that the MCP23017 and ADS1x15 I2C modules are correctly connected.

Other common issues and their solutions are:

- **Gamepad not recognized**: Try rebooting your Raspberry Pi. If the problem persists, ensure that the driver is being loaded at startup.
- **Buttons/joystick not responsive**: Double-check your wiring. If a button or joystick axis is not working, it may be due to a loose connection.
- **Compilation errors during software setup**: Ensure that the GCC Compiler is correctly installed on your Raspberry Pi.

Remember, you can use the I2C Tools for advanced troubleshooting (`sudo apt install -y i2c-tools`).


## Contributing

## License


![ADS1015](/images/ads1015.jpg) ![MCP23017](/images/mcp23017.jpg)

[video guide on othermod.com](https://othermod.com/analog-joystick-on-retropie/)
