#!/bin/bash

# Check if the script is running as root
if [ "$EUID" -ne 0 ]; then
  echo "This script requires root privileges. Please run it as root (e.g., using sudo)."
  exit 1
fi

# Check if Zenity is installed and install it if not
if ! command -v zenity &>/dev/null; then
  if command -v apt &>/dev/null; then
    sudo apt install -y zenity
  elif command -v yum &>/dev/null; then
    sudo yum install -y zenity
  else
    echo "Zenity is not installed, and we couldn't determine the package manager. Please install Zenity manually."
    exit 1
  fi
fi

# URL of the ISO to download
ISO_URL="https://archive.org/download/spoink-os/SpoinkOS-desktop-amd64-Blue.Whisker.Stable.iso"

# List available USB devices and prompt user for selection using Zenity
TARGET_DEVICE=$(zenity --list --column="Device" --column="Size" --column="Type" \
                     $(lsblk -d -o NAME,SIZE,TYPE | grep -e "disk" | awk '{print $1}') \
                     --title="Select USB Device" --text="Choose the target USB device:")

# Check if the user canceled the device selection
if [ -z "$TARGET_DEVICE" ]; then
  echo "Operation canceled. No device selected."
  exit 1
fi

# Check if the selected device exists
if [ -b "/dev/$TARGET_DEVICE" ]; then
  # Check if the selected device is mounted and unmount it
  if mount | grep -q "/dev/$TARGET_DEVICE"; then
    zenity --info --text="Unmounting /dev/$TARGET_DEVICE..."
    sudo umount "/dev/$TARGET_DEVICE"
  fi

  # Download the ISO file
  zenity --info --text="Downloading the ISO..."
  wget "$ISO_URL" -O spoink_os.iso

  # Flash the ISO to the selected device with a progress bar
  zenity --info --text="Flashing the ISO to /dev/$TARGET_DEVICE..."
  sudo dd if=spoink_os.iso of="/dev/$TARGET_DEVICE" bs=4M status=progress

  # Sync and eject the device
  sync
  sudo eject "/dev/$TARGET_DEVICE"

  zenity --info --text="Operation completed. The ISO has been flashed to /dev/$TARGET_DEVICE."

  # Clean up by removing the downloaded ISO file
  rm -f spoink_os.iso
else
  echo "Error: /dev/$TARGET_DEVICE does not exist."
fi
