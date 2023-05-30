#!/bin/bash

# https://www.othermod.com

function check_root() {
    if [ $(id -u) -ne 0 ]; then
        echo "Installer must be run as root."
        echo "Try 'sudo bash $0'"
        exit 1
    fi
}

function check_files_exist() {
    # Paths to the files
    local file_paths=("gamepad.c" "scan.c" "gamepad.service")

    # Check if the files exist
    for file_path in "${file_paths[@]}"; do
        if [ ! -f "$file_path" ]; then
            echo "File $file_path not found!"
            exit 1
        fi
    done
}

function enable_i2c() {
    local INTERACTIVE=False
    local BLACKLIST=/etc/modprobe.d/raspi-blacklist.conf
    local CONFIG=/boot/config.txt

    echo "Enabling I2C"

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

    grep 'dtparam=i2c_arm' /boot/config.txt >/dev/null
    if [ $? -eq 0 ]; then
        sudo sed -i '/dtparam=i2c_arm/c\dtparam=i2c_arm' /boot/config.txt
    else
        echo "dtparam=i2c_arm=on" >> /boot/config.txt
    fi
}

function compile_and_run_i2c_scanner() {
    echo "Compiling the I2C scanner"
    rm scan 2>/dev/null
    gcc -o scan scan.c
    if [ $? -eq 0 ]; then
        echo "Scanning every I2C bus to find the two modules"
    else
        echo "Failed to compile the I2C scanner"
				echo "Make sure you copied the correct scan.c file"
        exit 1
    fi
    ./scan
}

function get_and_set_i2c_bus() {
    # Scan /dev for available I2C buses
    available_buses=($(ls /dev/i2c-* 2>/dev/null | sed 's/\/dev\/i2c-//'))

    # Check if any I2C buses were found
    if [ ${#available_buses[@]} -eq 0 ]; then
        echo "No I2C buses found"
        echo "Try rebooting and run the script again"
        exit 1
    fi

    echo "List of available I2C buses: ${available_buses[@]}"
    echo "---------------------------------------------"
    echo "Using the above results as a guide, enter the number of the I2C bus"
    echo "The default is 1, but it depends on your setup"

    while true; do
        # Ask the user to input a value from the available buses
        read -p "Enter a number from the available buses: " input_value

        # Check if the input_value is in the list of available buses
        if [[ " ${available_buses[@]} " =~ " ${input_value} " ]]; then
            # Check for the existence of the line in gamepad.c
            if grep -q "#define I2C_BUS \"/dev/i2c-" "gamepad.c"; then
                # Use sed command to replace the line in gamepad.c
                sed -i "s|#define I2C_BUS \"/dev/i2c-.*\"|#define I2C_BUS \"/dev/i2c-$input_value\"|g" "gamepad.c"
                echo "The file gamepad.c has been updated successfully"
                break
            else
                echo "The line '#define I2C_BUS \"/dev/i2c-' does not exist in gamepad.c. Make sure you copied the correct file."
                exit 1
            fi
        else
            echo "Invalid input. The input should be a number from the available buses. Please try again."
        fi
    done
}


function install_service() {
    echo "Copying new driver and service files"
    cp -f gamepad /usr/bin/gamepad
    cp -f gamepad.service /etc/systemd/system/gamepad.service
    echo "Enabling gamepad service"
    systemctl enable gamepad
}

function compile_gamepad_driver() {
    echo "Compiling the gamepad driver"
    rm gamepad 2>/dev/null

    # Compile gamepad.c
    gcc -O3 -o gamepad gamepad.c
    if [ $? -eq 0 ]; then
        echo "Gamepad was compiled successfuly"
    else
        echo "Failed to compile the gamepad driver. Make sure you copied the correct gamepad.c file."
        exit 1
    fi
}

function handle_existing_gamepad_service() {
    echo "Disabling and removing existing gamepad service"
    systemctl stop gamepad 2>/dev/null
    systemctl disable gamepad 2>/dev/null
}

function prompt_for_autostart() {
    echo "Do you want the driver to load at startup?"
    PS3='Enter the number of your choice: '
    options=("Yes" "No")
    select yn in "${options[@]}"
    do
        case $yn in
            "Yes")
                install_service
                echo "A reboot is required"
                break;;
            "No")
                echo "You can run the driver with sudo ./gamepad &"
                break;;
            *)
                echo "Invalid option. Please enter the number corresponding to 'Yes' or 'No'.";;
        esac
    done
}

# Call functions in the right order
check_root
check_files_exist
enable_i2c
compile_and_run_i2c_scanner
get_and_set_i2c_bus
compile_gamepad_driver
handle_existing_gamepad_service
prompt_for_autostart
