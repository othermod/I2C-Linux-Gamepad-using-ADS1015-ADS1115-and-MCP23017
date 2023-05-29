#!/bin/bash
# https://www.othermod.com
if [ $(id -u) -ne 0 ]; then
	echo "Installer must be run as root."
	echo "Try 'sudo bash $0'"
	exit 1
fi

# Paths to the files
file_paths=("gamepad.c" "scan.c" "Makefile" "gamepad.service")

# Check if the files exist
for file_path in "${file_paths[@]}"; do
    if [ ! -f "$file_path" ]; then
        echo "File $file_path not found!"
        exit 1
    fi
done


echo "Enabling I2C"

INTERACTIVE=False
BLACKLIST=/etc/modprobe.d/raspi-blacklist.conf
CONFIG=/boot/config.txt

do_i2c() {
    SETTING=on
    STATUS=enabled

  #set_config_var dtparam=i2c_arm $SETTING $CONFIG &&
  if ! [ -e $BLACKLIST ]; then
    touch $BLACKLIST
  fi
  sed $BLACKLIST -i -e "s/^\(blacklist[[:space:]]*i2c[-_]bcm2708\)/#\1/"
  sed /etc/modules -i -e "s/^#[[:space:]]*\(i2c[-_]dev\)/\1/"
  if ! grep -q "^i2c[-_]dev" /etc/modules; then
    printf "i2c-dev\n" >> /etc/modules
  fi
  dtparam i2c_arm=$SETTING
  modprobe i2c-dev
}

do_i2c

grep 'dtparam=i2c_arm' /boot/config.txt >/dev/null
if [ $? -eq 0 ]; then
	sudo sed -i '/dtparam=i2c_arm/c\dtparam=i2c_arm' /boot/config.txt
else
	echo "dtparam=i2c_arm=on" >> /boot/config.txt
fi

echo "Compiling the i2c scanner."
rm scan 2>/dev/null
gcc -o scan scan.c
if [ $? -eq 0 ]; then
    echo "Scanning every i2c bus to find the two modules."
else
    echo "Failed to compile the i2c scanner. Make sure you copied the correct scan.c file."
    exit 1
fi

./scan

echo ""
echo "Using the above results as a guide, enter the number of the i2c bus."
echo "The default is 1, but it depends on your setup."

while true; do
    # Ask the user to input a value from 0 to 22
    read input_value

    # Check if the input_value is between 0 and 22
    if [[ "$input_value" =~ ^[0-9]+$ ]] && [ "$input_value" -ge 0 ] && [ "$input_value" -le 22 ]; then
        # Check for the existence of the line in gamepad.c
        if grep -q "#define I2C_BUS \"/dev/i2c-" "gamepad.c"; then
            # Use sed command to replace the line in gamepad.c
            sed -i "s|#define I2C_BUS \"/dev/i2c-.*\"|#define I2C_BUS \"/dev/i2c-$input_value\"|g" gamepad.c
            echo "The file gamepad.c has been updated successfully."
            break
        else
            echo "The line '#define I2C_BUS \"/dev/i2c-\"' does not exist in gamepad.c. Make sure you copied the correct file."
            break
        fi
    else
        echo "Invalid input. The input should be a number from 0 to 22. Please try again."
    fi
done

do_service() {
echo "Copying new driver and service files"
cp -f gamepad /usr/bin/gamepad
cp -f gamepad.service /etc/systemd/system/gamepad.service
echo "Enabling gamepad service"
systemctl enable gamepad
}

echo "Compiling the gamepad driver"
rm gamepad 2>/dev/null

# Compile gamepad.c
gcc -O3 -o gamepad gamepad.c
if [ $? -eq 0 ]; then
    echo "Compilation was successful. The file 'gamepad' was created."
else
    echo "Failed to compile the gamepad driver. Make sure you copied the correct gamepad.c file."
    exit 1
fi

echo "Disabling and removing existing gamepad service"
systemctl stop gamepad 2>/dev/null
systemctl disable gamepad 2>/dev/null
echo "Do you want the driver to load at startup (Enter 1 for Yes or 2 for No)?"
select yn in "Yes" "No"; do
    case $yn in
        Yes ) do_service; break;;
        No ) break;
    esac
done
