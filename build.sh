#!/bin/bash
set -e

ISO_NAME="WolfOS"
PROFILE_DIR="wolfos"
WORK_DIR="work"
OUT_DIR="out"

if [[ $EUID -ne 0 ]]; then
    echo "Run as root"
    exit 1
fi

pacman -Sy --noconfirm archiso git python python-pip calamares plasma sddm

rm -rf "$PROFILE_DIR" "$WORK_DIR" "$OUT_DIR"
mkdir -p "$PROFILE_DIR"

cp -r /usr/share/archiso/configs/releng/* "$PROFILE_DIR"

echo "$ISO_NAME" > "$PROFILE_DIR/airootfs/etc/hostname"

mkdir -p "$PROFILE_DIR/airootfs/root"
echo "/root/install_clctool.sh" > "$PROFILE_DIR/airootfs/root/.bash_profile"

cat << 'EOF' > "$PROFILE_DIR/airootfs/root/install_clctool.sh"
#!/bin/bash
pip install clctool
EOF

chmod +x "$PROFILE_DIR/airootfs/root/install_clctool.sh"

echo "plasma-meta" >> "$PROFILE_DIR/packages.x86_64"
echo "sddm" >> "$PROFILE_DIR/packages.x86_64"
echo "calamares" >> "$PROFILE_DIR/packages.x86_64"
echo "python-pip" >> "$PROFILE_DIR/packages.x86_64"

mkdir -p "$PROFILE_DIR/airootfs/etc/sddm.conf.d"
echo -e "[Autologin]\nUser=live\nSession=plasma.desktop" > "$PROFILE_DIR/airootfs/etc/sddm.conf.d/autologin.conf"

mkdir -p "$PROFILE_DIR/airootfs/etc/skel/.config/autostart"
echo -e "[Desktop Entry]\nType=Application\nExec=calamares\nHidden=false\nNoDisplay=false\nX-GNOME-Autostart-enabled=true\nName=Calamares Installer" > "$PROFILE_DIR/airootfs/etc/skel/.config/autostart/calamares.desktop"

mkdir -p "$PROFILE_DIR/calamares/branding/wolfos/images"
echo -e "name: WolfOS\nversion: 1.0\nproductName: WolfOS\nshortProductName: WolfOS\nversionedName: WolfOS Installer" > "$PROFILE_DIR/calamares/branding/wolfos/branding.desc"
cp /usr/share/pixmaps/archlinux-logo.png "$PROFILE_DIR/calamares/branding/wolfos/images/logo.png"

mkdir -p "$PROFILE_DIR/airootfs/etc/calamares"
echo -e "modules:\n  - welcome\n  - locale\n  - keyboard\n  - partition\n  - users\n  - summary\n  - install\n  - finished\nbranding: wolfos" > "$PROFILE_DIR/calamares/settings.conf"
cp -r "$PROFILE_DIR/calamares" "$PROFILE_DIR/airootfs/etc/calamares"

mkarchiso -v -w "$WORK_DIR" -o "$OUT_DIR" "$PROFILE_DIR"
