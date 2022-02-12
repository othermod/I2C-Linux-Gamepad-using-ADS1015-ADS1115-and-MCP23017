#!/bin/bash
# https://www.othermod.com
if [ $(id -u) -ne 0 ]; then
	echo "Installer must be run as root."
	echo "Try 'sudo bash $0'"
	exit 1
fi

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

echo "Checking status in config.txt"
grep '#dtparam=i2c_arm' /boot/config.txt >/dev/null
if [ $? -eq 0 ]; then
  echo "Enabling I2C_ARM in config.txt"
	sudo sed -i 's/#dtparam=i2c_arm/dtparam=i2c_arm/' /boot/config.txt

else
  echo "I2C_ARM already enabled in config.txt"
  sleep 1
fi

do_service() {
echo "Copying new driver and service files"
cp -f gamepad /usr/bin/gamepad
cp -f gamepad.service /etc/systemd/system/gamepad.service
echo "Enabling gamepad service"
systemctl enable gamepad
}


echo "Compiling the joystick driver"
make clean
make
echo "Disabling and removing existing gamepad service"
systemctl stop gamepad
systemctl disable gamepad
echo "Do you want the driver to load at startup (Enter 1 for Yes or 2 for No)?"
select yn in "Yes" "No"; do
    case $yn in
        Yes ) do_service; break;;
        No ) break;
    esac
done

echo "You must reboot to finish the installation"
