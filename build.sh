#!/bin/bash
set -euo pipefail

# === Configurable Variables ===
ISO_NAME="WolfOS"
PROFILE_DIR="wolfos"
WORK_DIR="work"
OUT_DIR="out"
HOSTNAME_FILE="airootfs/etc/hostname"
BASH_PROFILE="airootfs/root/.bash_profile"
INSTALL_SCRIPT="airootfs/root/install_clctool.sh"
AUTOSTART_DIR="airootfs/etc/skel/.config/autostart"
SDDM_CONF_DIR="airootfs/etc/sddm.conf.d"
USER="live"
FASTFETCH_DIR="airootfs/usr/bin"
OS_INFO_FILE="airootfs/etc/os-release"
PACMAN_CONFIG="airootfs/etc/pacman.conf"

# === Root Privileges Check ===
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root." >&2
    exit 1
fi

# === Dependencies ===
echo "[*] Installing required packages..."
pacman -Sy --noconfirm archiso git python python-pip sddm plasma-meta fastfetch zsh base-devel

# === Directory Reset ===
echo "[*] Cleaning up old build directories..."
rm -rf "$PROFILE_DIR" "$WORK_DIR" "$OUT_DIR"
mkdir -p "$PROFILE_DIR"

# === Copy Base Profile ===
echo "[*] Copying base releng profile..."
cp -r /usr/share/archiso/configs/releng/* "$PROFILE_DIR"

# === System Identity ===
echo "$ISO_NAME" > "$PROFILE_DIR/$HOSTNAME_FILE"

# === Set OS Info ===
echo "[*] Customizing OS information..."
cat << EOF > "$PROFILE_DIR/$OS_INFO_FILE"
NAME="$ISO_NAME"
ID=arch
VERSION="Rolling Release"
VERSION_ID="rolling"
PRETTY_NAME="$ISO_NAME"
ID_LIKE=arch
ANSI_COLOR="0;36"
CPE_NAME="cpe:/o:archlinux:archlinux"
HOME_URL="https://www.archlinux.org/"
SUPPORT_URL="https://bbs.archlinux.org/"
BUG_REPORT_URL="https://bugs.archlinux.org/"
EOF

# === Bootstrap Root Install Script ===
echo "[*] Creating pip install script for clctool..."
mkdir -p "$PROFILE_DIR/airootfs/root"
cat << 'EOF' > "$PROFILE_DIR/$INSTALL_SCRIPT"
#!/bin/bash
pip install clctool
EOF
chmod +x "$PROFILE_DIR/$INSTALL_SCRIPT"
echo "/root/install_clctool.sh" > "$PROFILE_DIR/$BASH_PROFILE"

# === User Creation and Setup ===
echo "[*] Creating Live User..."
useradd -m -G wheel -s /bin/zsh $USER

# Ensure the sudoers directory exists before adding the file
mkdir -p "$PROFILE_DIR/airootfs/etc/sudoers.d"

# Allow 'live' user to run sudo without a password prompt
echo "$USER ALL=(ALL) NOPASSWD: ALL" > "$PROFILE_DIR/airootfs/etc/sudoers.d/$USER"


# === Fastfetch Setup ===
echo "[*] Installing Fastfetch for system info..."
mkdir -p "$PROFILE_DIR/$FASTFETCH_DIR"
cat << 'EOF' > "$PROFILE_DIR/$FASTFETCH_DIR/fastfetch"
#!/bin/bash
echo "System: WolfOS"
echo "Kernel: $(uname -r)"
echo "Uptime: $(uptime -p)"
echo "Packages: $(pacman -Q | wc -l)"
EOF
chmod +x "$PROFILE_DIR/$FASTFETCH_DIR/fastfetch"

# === Zsh Setup ===
echo "[*] Configuring Zsh as the default shell..."
mkdir -p "$PROFILE_DIR/airootfs/root/.zshrc"
cat << 'EOF' > "$PROFILE_DIR/airootfs/root/.zshrc"
# Custom Zsh Config
autoload -Uz promptinit
promptinit
prompt pure
alias ll='ls -lah'
EOF

# === SDDM Configuration ===
echo "[*] Setting up SDDM autologin..."
mkdir -p "$PROFILE_DIR/$SDDM_CONF_DIR"
cat << EOF > "$PROFILE_DIR/$SDDM_CONF_DIR/autologin.conf"
[Autologin]
User=$USER
Session=plasma.desktop
EOF

# === Custom User Autostart ===
echo "[*] Adding autostart scripts for Live User..."
mkdir -p "$PROFILE_DIR/$AUTOSTART_DIR"
cat << EOF > "$PROFILE_DIR/$AUTOSTART_DIR/fastfetch.desktop"
[Desktop Entry]
Type=Application
Exec=fastfetch
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
Name=Fastfetch
EOF

# === Additional Packages ===
echo "[*] Adding custom packages..."
cat << EOF >> "$PROFILE_DIR/packages.x86_64"
plasma-meta
sddm
python-pip
zsh
base-devel
EOF

# === Customize Pacman Configuration ===
echo "[*] Modifying pacman config for better performance..."
cat << EOF >> "$PROFILE_DIR/$PACMAN_CONFIG"
[options]
ILoveCandy
ParallelDownloads = 5
EOF

# === Set Up Firewall (Optional) ===
echo "[*] Setting up firewall..."
mkdir -p "$PROFILE_DIR/airootfs/etc/systemd/system/firewalld.service.d"
cat << EOF > "$PROFILE_DIR/airootfs/etc/systemd/system/firewalld.service.d/override.conf"
[Service]
ExecStartPre=/usr/bin/firewalld --state-file /var/run/firewalld
EOF

# === Finalization ===
echo "[*] Finalizing configuration..."

# Remove unwanted files
rm -rf "$PROFILE_DIR/airootfs/usr/share/licenses"
rm -rf "$PROFILE_DIR/airootfs/usr/share/man"

# === Build ISO ===
echo "[*] Building ISO image: $ISO_NAME..."
mkarchiso -v -w "$WORK_DIR" -o "$OUT_DIR" "$PROFILE_DIR"

echo "[✔] ISO build completed: $OUT_DIR"
