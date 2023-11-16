#!/bin/bash
echo "Downloading ISO ..."
wget https://archive.org/download/spoink-os/SpoinkOS-desktop-amd64-Blue.Whisker.Stable.iso

set -e

# Get user input
iso_path="./SpoinkOS-desktop-amd64-Blue.Whisker.Stable.iso"

custom_wallpaper_path="./spoinkos.png"
#!/bin/bash

TARGET_DEVICE=$(lsblk -d -o NAME,SIZE,TYPE | grep -e "disk" | awk '{print $1}')

echo "Choose the target USB device (options: $TARGET_DEVICE):"
read SELECTED_DEVICE

# Create a temporary directory for customization
working_dir=$(mktemp -d)

# Mount the ISO in a non-restricted directory
sudo mount -o loop "$iso_path" "$working_dir"

# Copy the contents to a working directory
rsync -a "$working_dir"/ /tmp/custom_ubuntu/

# Remove Ubuntu and Kubuntu branding, and bloat
chroot /tmp/custom_ubuntu apt-get purge -y gnome-software gnome-software-common kubuntu-web-shortcuts muon \
    ubuntu-release-upgrader-gtk update-manager update-notifier update-notifier-common

# Remove unused packages
chroot /tmp/custom_ubuntu apt-get autoremove -y

# Set the wallpaper
mkdir -p /tmp/custom_ubuntu/usr/share/wallpapers/custom
cp "$custom_wallpaper_path" /tmp/custom_ubuntu/usr/share/wallpapers/custom/
chroot /tmp/custom_ubuntu kwriteconfig5 --file ~/.config/plasma-org.kde.plasma.desktop-appletsrc --group Containments --group 1 --group Wallpaper --group 1 --group org.kde.image --write Mainwallpaper /usr/share/wallpapers/custom/$(basename "$custom_wallpaper_path")

# Unmount the ISO
sudo umount "$working_dir"

# Create a custom GRUB configuration
cat <<EOF > /tmp/custom_ubuntu/boot/grub/custom.cfg
set default="0"
set timeout=5

menuentry "SpoinkOS" {
    linux /casper/vmlinuz boot=casper quiet splash ---
    initrd /casper/initrd
}
EOF

# Create the new ISO using xorriso
sudo xorriso -as mkisofs -o /tmp/custom_ubuntu.iso \
    -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin \
    -c isolinux/boot.cat -b isolinux/isolinux.bin -no-emul-boot \
    -boot-load-size 4 -boot-info-table \
    -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -isohybrid-gpt-basdat \
    /tmp/custom_ubuntu

# Cleanup
rm -rf "$working_dir"

echo "Flashing . . ."
  # Flash the ISO to the selected device with a progress bar
  sudo dd if=spoink_os.iso of="/dev/$SELECTED_DEVICE" bs=4M status=progress