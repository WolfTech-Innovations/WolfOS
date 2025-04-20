### 🛠️ Manual Installation Command List

1. **Identify the Target Disk**  
   Determine the disk where you want to install the OS:
   ```bash
   lsblk
   # Example output: /dev/sda
   # Replace /dev/sdX with your actual target disk
   ```

2. **Partition the Disk**  
   Create necessary partitions (e.g., root and swap):
   ```bash
   sudo fdisk /dev/sdX
   # Inside fdisk:
   # - 'g' to create a new GPT partition table
   # - 'n' to add a new partition (e.g., for root)
   # - 't' to set partition type (e.g., 20 for Linux filesystem)
   # - Repeat 'n' and 't' for additional partitions (e.g., swap)
   # - 'w' to write changes and exit
   ```

3. **Format the Partitions**  
   Format the created partitions:
   ```bash
   sudo mkfs.ext4 /dev/sdX1  # Format root partition
   sudo mkswap /dev/sdX2     # Format swap partition
   ```

4. **Mount the Root Partition**  
   Mount the root partition to `/mnt`:
   ```bash
   sudo mount /dev/sdX1 /mnt
   ```

5. **Extract the Root Filesystem**  
   If you have a SquashFS image:
   ```bash
   sudo unsquashfs -f -d /mnt /path/to/rootfs.squashfs
   ```
   If you have a directory:
   ```bash
   sudo cp -a /path/to/rootfs/* /mnt/
   ```

6. **Mount System Directories**  
   Prepare for chroot by mounting necessary filesystems:
   ```bash
   sudo mount --bind /dev /mnt/dev
   sudo mount --bind /proc /mnt/proc
   sudo mount --bind /sys /mnt/sys
   ```

7. **Chroot into the New System**  
   Enter the new system environment:
   ```bash
   sudo chroot /mnt
   ```

8. **Install GRUB Bootloader**  
   Install GRUB to make the system bootable:
   ```bash
   grub-install /dev/sdX
   update-grub
   ```

9. **Configure fstab**  
   Generate and edit `/etc/fstab` with correct UUIDs:
   ```bash
   blkid
   # Use the output to populate /etc/fstab accordingly
   ```

10. **Exit and Unmount**  
    Exit chroot and unmount filesystems:
    ```bash
    exit
    sudo umount /mnt/dev
    sudo umount /mnt/proc
    sudo umount /mnt/sys
    sudo umount /mnt
    ```

11. **Reboot**  
    Restart the system:
    ```bash
    sudo reboot
    ```

---
