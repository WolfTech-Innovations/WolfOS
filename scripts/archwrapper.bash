#!/bin/bash

set -e

# Paths
BASE_DIR="/mnt/data"
SRC_DIR="$BASE_DIR/archwrap"
TUI_DIR="$BASE_DIR/archwrap_tui"
INSTALL_DIR="/usr/local/bin"
EXEC_NAME="archwrap"

echo "[*] Copying archwrap base project to archwrap_tui..."
rm -rf "$TUI_DIR"
cp -r "$SRC_DIR" "$TUI_DIR"

echo "[*] Writing TUI main.c..."
cat > "$TUI_DIR/src/main.c" << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include "archwrap.h"

int main() {
    char pkg[256];

    initscr();
    printw("ArchWrap TUI Installer\n");
    printw("Enter Arch/BlackArch package name: ");
    refresh();
    getstr(pkg);
    endwin();

    init_dirs();
    setup_blackarch();

    install_with_deps(pkg);

    return 0;
}
EOF

echo "[*] Writing TUI install.c..."
cat > "$TUI_DIR/src/install.c" << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <archive.h>
#include <archive_entry.h>
#include "archwrap.h"

int extract_package(const char *pkg) {
    char path[512];
    snprintf(path, sizeof(path), "/tmp/archwrap/%s.pkg.tar.zst", pkg);

    struct archive *a = archive_read_new();
    struct archive *ext = archive_write_disk_new();
    struct archive_entry *entry;

    archive_read_support_format_tar(a);
    archive_read_support_filter_zstd(a);
    if (archive_read_open_filename(a, path, 10240)) return -1;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        archive_write_header(ext, entry);
        const void *buff;
        size_t size;
        int64_t offset;
        while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK)
            archive_write_data_block(ext, buff, size, offset);
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    return 0;
}

// Simulate extracting real deps from desc
char **get_deps(const char *pkg) {
    FILE *f;
    static char *deps[10];
    int i = 0;
    char line[256];
    char desc_path[512];

    snprintf(desc_path, sizeof(desc_path), "/tmp/archwrap/%s.desc", pkg);
    f = fopen(desc_path, "r");
    if (!f) {
        deps[0] = NULL;
        return deps;
    }

    int record = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "%DEPENDS%")) {
            record = 1;
            continue;
        }
        if (line[0] == '%' || line[0] == '\n') {
            record = 0;
        }
        if (record && i < 9) {
            deps[i] = strdup(line);
            deps[i][strcspn(deps[i], "\n")] = 0; // strip newline
            i++;
        }
    }
    deps[i] = NULL;
    fclose(f);
    return deps;
}

void install_with_deps(const char *pkg) {
    if (pkg_installed(pkg)) {
        printf("[=] %s already installed\n", pkg);
        return;
    }

    if (download_package(pkg) != 0) {
        printf("[-] Failed to download %s\n", pkg);
        return;
    }

    if (extract_package(pkg) != 0) {
        printf("[-] Failed to extract %s\n", pkg);
        return;
    }

    cache_package(pkg);
    printf("[+] Installed %s\n", pkg);

    char **deps = get_deps(pkg);
    for (int i = 0; deps[i]; ++i) {
        install_with_deps(deps[i]);
        free(deps[i]);
    }
}
EOF

echo "[*] Writing CMakeLists.txt with ncurses link..."
cat > "$TUI_DIR/CMakeLists.txt" << 'EOF'
cmake_minimum_required(VERSION 3.10)
project(archwrap C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_executable(archwrap
    src/main.c
    src/download.c
    src/install.c
    src/blackarch.c
    src/utils.c
)

target_link_libraries(archwrap archive curl zstd ncurses)
EOF

echo "[*] Building archwrap..."
cd "$TUI_DIR"
mkdir -p build
cd build
cmake ..
make -j$(nproc)

echo "[*] Installing archwrap to $INSTALL_DIR..."
sudo cp "$EXEC_NAME" "$INSTALL_DIR/"

echo "[*] Making sure $EXEC_NAME is executable..."
sudo chmod +x "$INSTALL_DIR/$EXEC_NAME"

echo "[*] Done! You can run archwrap now by typing: $EXEC_NAME"
