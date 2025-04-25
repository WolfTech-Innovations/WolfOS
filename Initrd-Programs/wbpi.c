/*
 * WBPI - Wolf Binary Packaging Installer v0.2
 * by WolfTech Innovations
 *
 * A simple package manager for WBP files written in C.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define DB_PATH "/var/lib/wbpi"
#define INSTALLED_PATH DB_PATH "/installed"
#define FILES_PATH DB_PATH "/files"
#define TEMPLATE_DIR "./wbp-template"

void ensure_dirs() {
    mkdir(DB_PATH, 0755);
    mkdir(INSTALLED_PATH, 0755);
    mkdir(FILES_PATH, 0755);
}

void install_package(const char *wbp_file) {
    char tmp_dir[] = "/tmp/wbpi-XXXXXX";
    if (!mkdtemp(tmp_dir)) {
        perror("mkdtemp");
        exit(1);
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "tar --use-compress-program=unzstd -xf %s -C %s", wbp_file, tmp_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to extract package.\n");
        exit(1);
    }

    char info_path[512];
    snprintf(info_path, sizeof(info_path), "%s/CONTROL/info.json", tmp_dir);

    FILE *info = fopen(info_path, "r");
    if (!info) {
        perror("info.json");
        exit(1);
    }

    char buf[4096];
    fread(buf, sizeof(char), sizeof(buf), info);
    fclose(info);

    char name[128];
    sscanf(buf, "{\"package\": \"%127[^\"]", name);

    char installed_meta[512];
    snprintf(installed_meta, sizeof(installed_meta), "%s/%s.json", INSTALLED_PATH, name);

    FILE *meta_out = fopen(installed_meta, "w");
    if (!meta_out) {
        perror("install metadata");
        exit(1);
    }
    fwrite(buf, sizeof(char), strlen(buf), meta_out);
    fclose(meta_out);

    snprintf(cmd, sizeof(cmd), "bash %s/CONTROL/preinst.sh 2>/dev/null", tmp_dir);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "find %s/DATA -type f | sed \"s|%s/DATA||\" > %s/%s.files", tmp_dir, tmp_dir, FILES_PATH, name);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "cp -r %s/DATA/* /", tmp_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "File copy failed.\n");
        exit(1);
    }

    snprintf(cmd, sizeof(cmd), "bash %s/CONTROL/postinst.sh 2>/dev/null", tmp_dir);
    system(cmd);

    printf("[WBPI] %s installed successfully.\n", name);
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmp_dir);
    system(cmd);
}

void remove_package(const char *name) {
    char filelist[512];
    snprintf(filelist, sizeof(filelist), "%s/%s.files", FILES_PATH, name);

    FILE *f = fopen(filelist, "r");
    if (!f) {
        fprintf(stderr, "Package '%s' not found.\n", name);
        exit(1);
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "/%s", line);
        remove(fullpath);
    }
    fclose(f);

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/%s.json", INSTALLED_PATH, name);
    remove(meta_path);
    remove(filelist);

    printf("[WBPI] %s removed.\n", name);
}

void list_packages() {
    system("ls " INSTALLED_PATH " | sed 's/.json$//'");
}

void show_info(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.json", INSTALLED_PATH, name);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cat %s", path);
    system(cmd);
}

void create_template(const char *name) {
    char cmd[1024];

    // Create CONTROL and DATA directories
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/CONTROL' '%s/DATA'", name, name);
    system(cmd);

    // Create info.json file
    snprintf(cmd, sizeof(cmd),
             "echo '{\"package\": \"%s\", \"version\": \"1.0\", \"description\": \"Sample WBP Package\"}' > '%s/CONTROL/info.json'",
             name, name);
    system(cmd);

    // Create preinst.sh and postinst.sh
    snprintf(cmd, sizeof(cmd), "touch '%s/CONTROL/preinst.sh' '%s/CONTROL/postinst.sh'", name, name);
    system(cmd);

    // Confirmation message
    printf("[WBPI] Template created at %s\n", name);
}

int main(int argc, char **argv) {
    ensure_dirs();

    if (argc < 2) {
        fprintf(stderr, "Usage: %s {install <file.wbp> | remove <name> | list | info <name> | create <name>}\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "install") == 0 && argc == 3)
        install_package(argv[2]);
    else if (strcmp(argv[1], "remove") == 0 && argc == 3)
        remove_package(argv[2]);
    else if (strcmp(argv[1], "list") == 0)
        list_packages();
    else if (strcmp(argv[1], "info") == 0 && argc == 3)
        show_info(argv[2]);
    else if (strcmp(argv[1], "create") == 0 && argc == 3)
        create_template(argv[2]);
    else {
        fprintf(stderr, "Invalid command or usage.\n");
        return 1;
    }

    return 0;
}
