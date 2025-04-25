#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <regex.h>

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BG_BLUE "\033[44m"

#define MAX_COMMANDS 500
#define MAX_COMMAND_LENGTH 64
#define MAX_CATEGORIES 10
#define MAX_DESCRIPTION_LENGTH 256
#define MAX_HELP_TEXT_LENGTH 2048

typedef struct {
    char name[MAX_COMMAND_LENGTH];
    char category[32];
    char description[MAX_DESCRIPTION_LENGTH];
    int is_built_in;
} Command;

typedef struct {
    char name[32];
    char color[16];
    int command_count;
} Category;

Command command_list[MAX_COMMANDS];
Category categories[MAX_CATEGORIES] = {
    {"System", GREEN, 0},
    {"Files", CYAN, 0},
    {"Network", BLUE, 0},
    {"Development", MAGENTA, 0},
    {"Text", YELLOW, 0},
    {"Media", RED, 0},
    {"WolfOS", GREEN, 0},
    {"Learn", CYAN, 0},
    {"Utilities", WHITE, 0},
    {"Other", WHITE, 0}
};

int command_count = 0;
int current_view = 0; // 0 = command list, 1 = help view
char current_help_text[MAX_HELP_TEXT_LENGTH] = "";
char current_help_title[MAX_COMMAND_LENGTH] = "";

void handle_sigint(int sig);
void display_splash();
void animate_loading(int fast_mode);
void fancy_print(const char* text, const char* color, int delay_ms);
void show_progress_bar(const char* text, int duration_ms);
void print_system_info();
int check_internet_connection();
void refresh_command_list();
void categorize_commands();
void draw_tui(int selected_index, int start_idx, int category_filter);
void execute_selected_command(const Command* command, const char* args);
int get_terminal_width();
int get_terminal_height();
void load_command_descriptions();
void show_command_help(const char* command);
void launch_learning_module(const char* topic);

int main(int argc, char* argv[]) {
    srand(time(NULL));
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    
    int fast_boot = 0;
    if (argc > 1 && strcmp(argv[1], "--fast") == 0) {
        fast_boot = 1;
    }
    
    if (!fast_boot) {
        display_splash();
        animate_loading(0);
        show_progress_bar("Setting up environment", 300);
    }
    
    print_system_info();
    check_internet_connection();
    
    if (!fast_boot) {
        printf("\n%s%s╔════════════════════════════════════════════╗%s\n", BOLD, BLUE, RESET);
        printf("%s%s║ %sWolfOS is ready                         %s║%s\n", BOLD, BLUE, GREEN, BLUE, RESET);
        printf("%s%s╚════════════════════════════════════════════╝%s\n\n", BOLD, BLUE, RESET);
    }
    
    refresh_command_list();
    categorize_commands();
    load_command_descriptions();
    
    int selected_index = 0;
    int start_idx = 0;
    int category_filter = -1; // -1 means show all
    char args[1024] = "";
    
    struct termios orig_termios, raw_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw_termios = orig_termios;
    raw_termios.c_lflag &= ~(ICANON | ECHO);
    
    while (1) {
        int term_height = get_terminal_height();
        int visible_commands = term_height - 10;
        
        if (visible_commands < 3) visible_commands = 3;
        
        printf("\033[2J\033[H");
        
        if (current_view == 0) {
            draw_tui(selected_index, start_idx, category_filter);
            
            if (category_filter == -1) {
                printf("\n%s%sCommand:%s %s", BOLD, GREEN, RESET, command_list[selected_index].name);
                if (strlen(command_list[selected_index].description) > 0) {
                    printf(" - %s", command_list[selected_index].description);
                }
            } else {
                int actual_index = -1;
                int count = 0;
                for (int i = 0; i < command_count; i++) {
                    if (strcmp(command_list[i].category, categories[category_filter].name) == 0) {
                        if (count == selected_index) {
                            actual_index = i;
                            break;
                        }
                        count++;
                    }
                }
                
                if (actual_index >= 0) {
                    printf("\n%s%sCommand:%s %s", BOLD, GREEN, RESET, command_list[actual_index].name);
                    if (strlen(command_list[actual_index].description) > 0) {
                        printf(" - %s", command_list[actual_index].description);
                    }
                }
            }
            
            printf("\n%s%sArguments:%s %s", BOLD, CYAN, RESET, args);
            printf("\n\n%s%s[↑/↓] Navigate  [Enter] Execute  [Tab] Args  [Space] Categories  [H] Help  [L] Learn  [Q] Quit%s", BOLD, YELLOW, RESET);
        } else {
            // Help view
            int width = get_terminal_width();
            printf("%s%s", BOLD, BLUE);
            for (int i = 0; i < width; i++) printf("═");
            printf("%s\n", RESET);
            
            printf("%s%s %s - Help %s\n", BOLD, CYAN, current_help_title, RESET);
            
            printf("%s", BLUE);
            for (int i = 0; i < width; i++) printf("═");
            printf("%s\n\n", RESET);
            
            printf("%s", current_help_text);
            
            printf("\n\n%s%s[Esc] Back to command list%s", BOLD, YELLOW, RESET);
        }
        
        tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);
        char key = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        
        if (current_view == 0) {
            // Command list view
            switch (key) {
                case 'q':
                case 'Q':
                    fancy_print("Exiting WolfOS...", BLUE, 10);
                    return 0;
                    
                case 'r':
                case 'R':
                    fancy_print("Refreshing command list...", BLUE, 10);
                    refresh_command_list();
                    categorize_commands();
                    load_command_descriptions();
                    selected_index = 0;
                    start_idx = 0;
                    category_filter = -1;
                    break;
                    
                case '\t':
                    printf("\n%s%sEnter arguments:%s ", BOLD, CYAN, RESET);
                    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
                    fflush(stdin);
                    if (fgets(args, sizeof(args), stdin) != NULL) {
                        args[strcspn(args, "\n")] = 0;
                    }
                    break;
                    
                case ' ':
                    category_filter++;
                    if (category_filter >= MAX_CATEGORIES) category_filter = -1;
                    selected_index = 0;
                    start_idx = 0;
                    break;
                    
                case 'h':
                case 'H':
                    if (category_filter == -1) {
                        show_command_help(command_list[selected_index].name);
                    } else {
                        int actual_index = -1;
                        int count = 0;
                        for (int i = 0; i < command_count; i++) {
                            if (strcmp(command_list[i].category, categories[category_filter].name) == 0) {
                                if (count == selected_index) {
                                    actual_index = i;
                                    break;
                                }
                                count++;
                            }
                        }
                        
                        if (actual_index >= 0) {
                            show_command_help(command_list[actual_index].name);
                        }
                    }
                    current_view = 1;
                    break;
                    
                case 'l':
                case 'L':
                    if (category_filter == -1) {
                        launch_learning_module(command_list[selected_index].name);
                    } else {
                        int actual_index = -1;
                        int count = 0;
                        for (int i = 0; i < command_count; i++) {
                            if (strcmp(command_list[i].category, categories[category_filter].name) == 0) {
                                if (count == selected_index) {
                                    actual_index = i;
                                    break;
                                }
                                count++;
                            }
                        }
                        
                        if (actual_index >= 0) {
                            launch_learning_module(command_list[actual_index].name);
                        }
                    }
                    current_view = 1;
                    break;
                    
                case '\n':
                case '\r':
                    if (category_filter == -1) {
                        execute_selected_command(&command_list[selected_index], args);
                    } else {
                        int actual_index = -1;
                        int count = 0;
                        for (int i = 0; i < command_count; i++) {
                            if (strcmp(command_list[i].category, categories[category_filter].name) == 0) {
                                if (count == selected_index) {
                                    actual_index = i;
                                    break;
                                }
                                count++;
                            }
                        }
                        
                        if (actual_index >= 0) {
                            execute_selected_command(&command_list[actual_index], args);
                        }
                    }
                    printf("\n%s%sPress any key to continue...%s", BOLD, YELLOW, RESET);
                    tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);
                    getchar();
                    break;
                    
                case 65: // Up arrow
                    selected_index--;
                    if (selected_index < 0) {
                        if (category_filter == -1) {
                            selected_index = command_count - 1;
                        } else {
                            selected_index = categories[category_filter].command_count - 1;
                        }
                    }
                    
                    if (selected_index < start_idx) 
                        start_idx = selected_index;
                    if (selected_index >= start_idx + visible_commands) 
                        start_idx = selected_index - visible_commands + 1;
                    break;
                    
                case 66: // Down arrow
                    selected_index++;
                    if (category_filter == -1) {
                        if (selected_index >= command_count) selected_index = 0;
                    } else {
                        if (selected_index >= categories[category_filter].command_count) selected_index = 0;
                    }
                    
                    if (selected_index < start_idx) 
                        start_idx = selected_index;
                    if (selected_index >= start_idx + visible_commands) 
                        start_idx = selected_index - visible_commands + 1;
                    break;
            }
        } else {
            // Help view
            if (key == 27) { // ESC key
                current_view = 0;
            }
        }
    }
    
    return 0;
}

void refresh_command_list() {
    command_count = 0;
    
    // Add built-in commands
    strcpy(command_list[command_count].name, "refresh");
    strcpy(command_list[command_count].category, "WolfOS");
    strcpy(command_list[command_count].description, "Refresh command list");
    command_list[command_count].is_built_in = 1;
    command_count++;
    
    strcpy(command_list[command_count].name, "sysinfo");
    strcpy(command_list[command_count].category, "WolfOS");
    strcpy(command_list[command_count].description, "Show system information");
    command_list[command_count].is_built_in = 1;
    command_count++;
    
    strcpy(command_list[command_count].name, "clear");
    strcpy(command_list[command_count].category, "WolfOS");
    strcpy(command_list[command_count].description, "Clear the screen");
    command_list[command_count].is_built_in = 1;
    command_count++;
    
    strcpy(command_list[command_count].name, "restart");
    strcpy(command_list[command_count].category, "WolfOS");
    strcpy(command_list[command_count].description, "Restart WolfOS");
    command_list[command_count].is_built_in = 1;
    command_count++;
    
    strcpy(command_list[command_count].name, "learn-bash");
    strcpy(command_list[command_count].category, "Learn");
    strcpy(command_list[command_count].description, "Interactive Bash tutorial");
    command_list[command_count].is_built_in = 1;
    command_count++;
    
    strcpy(command_list[command_count].name, "learn-linux");
    strcpy(command_list[command_count].category, "Learn");
    strcpy(command_list[command_count].description, "Learn Linux basics");
    command_list[command_count].is_built_in = 1;
    command_count++;
    
    strcpy(command_list[command_count].name, "learn-vim");
    strcpy(command_list[command_count].category, "Learn");
    strcpy(command_list[command_count].description, "Interactive Vim tutorial");
    command_list[command_count].is_built_in = 1;
    command_count++;
    
    strcpy(command_list[command_count].name, "command-quiz");
    strcpy(command_list[command_count].category, "Learn");
    strcpy(command_list[command_count].description, "Test your command knowledge");
    command_list[command_count].is_built_in = 1;
    command_count++;
    
    // Get system commands
    const char *paths[] = {"/bin", "/usr/bin", "/sbin", "/usr/sbin", NULL};
    
    for (int p = 0; paths[p] != NULL; p++) {
        DIR *dir = opendir(paths[p]);
        if (dir != NULL) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL && command_count < MAX_COMMANDS) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || entry->d_name[0] == '.')
                    continue;
                
                // Check if command already exists in our list
                int exists = 0;
                for (int i = 0; i < command_count; i++) {
                    if (strcmp(command_list[i].name, entry->d_name) == 0) {
                        exists = 1;
                        break;
                    }
                }
                
                // Add to command list if not a duplicate
                if (!exists) {
                    strncpy(command_list[command_count].name, entry->d_name, MAX_COMMAND_LENGTH - 1);
                    command_list[command_count].name[MAX_COMMAND_LENGTH - 1] = '\0';
                    command_list[command_count].is_built_in = 0;
                    command_count++;
                }
            }
            closedir(dir);
        }
    }
    
    // Sort the commands alphabetically
    for (int i = 0; i < command_count - 1; i++) {
        for (int j = i + 1; j < command_count; j++) {
            if (strcmp(command_list[i].name, command_list[j].name) > 0) {
                Command temp = command_list[i];
                command_list[i] = command_list[j];
                command_list[j] = temp;
            }
        }
    }
    
    printf("%s%sFound %d commands%s\n", BOLD, GREEN, command_count, RESET);
}

void categorize_commands() {
    // Reset category counts
    for (int c = 0; c < MAX_CATEGORIES; c++) {
        categories[c].command_count = 0;
    }
    
    // Define patterns for each category
    const char *patterns[][2] = {
        {"System", "^(systemctl|journalctl|modprobe|lsmod|uname|hostname|ps|top|htop|free|df|du|mount|umount|fdisk|parted|lsblk|lsusb|lspci|dmesg|shutdown|reboot|halt|init|service)$"},
        {"Files", "^(ls|cd|cp|mv|rm|mkdir|rmdir|touch|cat|less|more|head|tail|find|grep|sed|awk|sort|uniq|wc|diff|patch|chmod|chown|chgrp|ln|file|dd)$"},
        {"Network", "^(ping|traceroute|netstat|ss|ip|ifconfig|route|arp|ssh|scp|ftp|wget|curl|telnet|nslookup|dig|host|whois|nc|tcpdump|iptables|firewall-cmd)$"},
        {"Development", "^(gcc|g\\+\\+|make|cmake|git|svn|python|python3|perl|ruby|java|javac|node|npm|pip|go|rustc|cargo|clang|as|ld|ar|nm|objdump|strip|gdb|valgrind)$"},
        {"Text", "^(vim|nano|emacs|vi|ed|ex|pico|joe|jed|gedit|kate|kwrite|mousepad|leafpad|pluma|xed|micro|ne|mcedit|hexedit|xxd)$"},
        {"Media", "^(ffmpeg|mplayer|vlc|mpv|sox|aplay|arecord|convert|mogrify|display|gimp|inkscape|xpdf|evince|eog|feh|imagemagick)$"},
        {"Utilities", "^(tar|gzip|bzip2|xz|zip|unzip|7z|cal|date|time|bc|expr|dc|units|factor|seq|yes|true|false|which|whereis|locate|sudo|su|passwd|chpasswd|who|w|last|id|groups)$"}
    };
    
    // Apply patterns to categorize commands
    for (int i = 0; i < command_count; i++) {
        // Skip already categorized commands
        if (strlen(command_list[i].category) > 0) {
            // Count this command in its category
            for (int c = 0; c < MAX_CATEGORIES; c++) {
                if (strcmp(command_list[i].category, categories[c].name) == 0) {
                    categories[c].command_count++;
                    break;
                }
            }
            continue;
        }
        
        int categorized = 0;
        for (int p = 0; p < sizeof(patterns) / sizeof(patterns[0]); p++) {
            regex_t regex;
            int ret = regcomp(&regex, patterns[p][1], REG_EXTENDED | REG_NOSUB);
            if (ret == 0) {
                ret = regexec(&regex, command_list[i].name, 0, NULL, 0);
                if (ret == 0) {
                    // Match found
                    strcpy(command_list[i].category, patterns[p][0]);
                    categorized = 1;
                    
                    // Increment category count
                    for (int c = 0; c < MAX_CATEGORIES; c++) {
                        if (strcmp(categories[c].name, patterns[p][0]) == 0) {
                            categories[c].command_count++;
                            break;
                        }
                    }
                    
                    regfree(&regex);
                    break;
                }
                regfree(&regex);
            }
        }
        
        // If not categorized, put in Other
        if (!categorized) {
            strcpy(command_list[i].category, "Other");
            categories[MAX_CATEGORIES - 1].command_count++;
        }
    }
}

void load_command_descriptions() {
    // Add descriptions for common commands
    const char *desc[][2] = {
        {"ls", "List directory contents"},
        {"cd", "Change the current directory"},
        {"cp", "Copy files and directories"},
        {"mv", "Move/rename files and directories"},
        {"rm", "Remove files or directories"},
        {"mkdir", "Create new directories"},
        {"grep", "Search for patterns in files"},
        {"find", "Search for files in a directory hierarchy"},
        {"cat", "Concatenate and display file content"},
        {"pwd", "Print working directory"},
        {"chmod", "Change file permissions"},
        {"chown", "Change file owner and group"},
        {"ps", "Report process status"},
        {"top", "Display Linux tasks"},
        {"ping", "Send ICMP ECHO_REQUEST to network hosts"},
        {"ssh", "Secure Shell client"},
        {"wget", "Download files from the web"},
        {"curl", "Transfer data from or to a server"},
        {"git", "Distributed version control system"},
        {"tar", "Archive utility"},
        {"sudo", "Execute a command as another user"},
        {"apt", "Package management utility"},
        {"vi", "Text editor"},
        {"nano", "Simple text editor"},
        {"gcc", "GNU C/C++ compiler"},
        {"make", "Build automation tool"},
        {"python", "Python programming language interpreter"},
        {"man", "Display manual pages"},
        {"less", "View file contents page by page"},
        {"head", "Output the first part of files"},
        {"tail", "Output the last part of files"},
        {"df", "Report file system disk space usage"},
        {"du", "Estimate file space usage"},
        {"free", "Display memory usage"},
        {"htop", "Interactive process viewer"},
        {"systemctl", "Control the systemd system manager"},
        {"journalctl", "Query the systemd journal"},
        {"ifconfig", "Configure network interfaces"},
        {"ip", "Show/manipulate routing, devices, and tunnels"},
        {"awk", "Pattern scanning and processing language"},
        {"sed", "Stream editor for filtering and transforming text"},
        {"sort", "Sort lines of text files"},
        {"uniq", "Report or filter out repeated lines"},
        {"diff", "Compare files line by line"},
        {"date", "Display or set date and time"},
        {"uname", "Print system information"},
        {"who", "Show who is logged on"},
        {"wc", "Print newline, word, and byte counts"},
        {"touch", "Change file timestamps or create empty files"},
        {"ln", "Make links between files"}
    };
    
    for (int i = 0; i < command_count; i++) {
        // Skip commands that already have descriptions
        if (strlen(command_list[i].description) > 0) {
            continue;
        }
        
        for (int d = 0; d < sizeof(desc) / sizeof(desc[0]); d++) {
            if (strcmp(command_list[i].name, desc[d][0]) == 0) {
                strcpy(command_list[i].description, desc[d][1]);
                break;
            }
        }
    }
}

void draw_tui(int selected_index, int start_idx, int category_filter) {
    int term_width = get_terminal_width();
    int term_height = get_terminal_height();
    int visible_commands = term_height - 12;
    
    if (visible_commands < 3) visible_commands = 3;
    
    printf("%s%s", BOLD, BLUE);
    for (int i = 0; i < term_width; i++) printf("═");
    printf("%s\n", RESET);
    
    if (category_filter == -1) {
        printf("%s%s WolfOS Command Interface - %d commands available %s\n", BOLD, BG_BLUE, command_count, RESET);
    } else {
        printf("%s%s Category: %s - %d commands %s\n", BOLD, BG_BLUE, categories[category_filter].name, categories[category_filter].command_count, RESET);
    }
    
    printf("%s", BLUE);
    for (int i = 0; i < term_width; i++) printf("═");
    printf("%s\n\n", RESET);
    
    // Show categories bar
    printf("%s%s Categories: %s", BOLD, YELLOW, RESET);
    for (int c = 0; c < MAX_CATEGORIES; c++) {
        if (c == category_filter) {
            printf("%s%s[%s]%s ", BOLD, categories[c].color, categories[c].name, RESET);
        } else {
            printf("%s%s%s%s ", categories[c].color, categories[c].name, RESET, (c < MAX_CATEGORIES - 1) ? "|" : "");
        }
    }
    printf("\n\n");
    
    if (category_filter == -1) {
        // Show all commands
        int end_idx = start_idx + visible_commands;
        if (end_idx > command_count) end_idx = command_count;
        
        for (int i = start_idx; i < end_idx; i++) {
            const char* category_color = WHITE;
            for (int c = 0; c < MAX_CATEGORIES; c++) {
                if (strcmp(command_list[i].category, categories[c].name) == 0) {
                    category_color = categories[c].color;
                    break;
                }
            }
            
            if (i == selected_index) {
                printf("%s%s> %s%-15s %s%s%-10s%s", BOLD, GREEN, YELLOW, command_list[i].name, 
                       category_color, command_list[i].category, RESET, 
                       command_list[i].is_built_in ? " [Built-in]" : "");
                
                if (strlen(command_list[i].description) > 0) {
                    int desc_len = strlen(command_list[i].description);
                    int max_desc = term_width - 30;
                    if (desc_len > max_desc) {
                        // Truncate description
                        printf(" - %.25s...", command_list[i].description);
                    } else {
                        printf(" - %s", command_list[i].description);
                    }
                }
                printf("%s\n", RESET);
            } else {
                printf("  %-15s %s%-10s%s", command_list[i].name, category_color, 
                       command_list[i].category, RESET);
                
                if (command_list[i].is_built_in) {
                    printf(" [Built-in]");
                }
                
                if (strlen(command_list[i].description) > 0) {
                    int desc_len = strlen(command_list[i].description);
                    int max_desc = term_width - 30;
                    if (desc_len > max_desc) {
                        // Truncate description
                        printf(" - %.25s...", command_list[i].description);
                    } else {
                        printf(" - %s", command_list[i].description);
                    }
                }
                printf("\n");
            }
        }
        
        // Show pagination info if needed
        if (command_count > visible_commands) {
            printf("\n%s%sShowing %d-%d of %d commands%s\n", 
                   BOLD, BLUE, start_idx + 1, end_idx, command_count, RESET);
        }
    } else {
        // Show commands for selected category
        int category_command_count = 0;
        int filtered_commands[MAX_COMMANDS];
        
        // First, collect all commands in this category
        for (int i = 0; i < command_count; i++) {
            if (strcmp(command_list[i].category, categories[category_filter].name) == 0) {
                filtered_commands[category_command_count++] = i;
            }
        }
        
        int end_idx = start_idx + visible_commands;
        if (end_idx > category_command_count) end_idx = category_command_count;
        
        for (int i = start_idx; i < end_idx; i++) {
            int cmd_idx = filtered_commands[i];
            
            if (i == selected_index) {
                printf("%s%s> %s%-15s", BOLD, GREEN, YELLOW, command_list[cmd_idx].name);
                
                if (command_list[cmd_idx].is_built_in) {
                    printf(" [Built-in]");
                }
                
                if (strlen(command_list[cmd_idx].description) > 0) {
                    printf(" - %s", command_list[cmd_idx].description);
                }
                printf("%s\n", RESET);
            } else {
                printf("  %-15s", command_list[cmd_idx].name);
                
                if (command_list[cmd_idx].is_built_in) {
                    printf(" [Built-in]");
                }
                
                if (strlen(command_list[cmd_idx].description) > 0) {
                    printf(" - %s", command_list[cmd_idx].description);
                }
                printf("\n"); 
                }
        }
        
        // Show pagination info if needed
        if (category_command_count > visible_commands) {
            printf("\n%s%sShowing %d-%d of %d commands in %s category%s\n", 
                   BOLD, BLUE, start_idx + 1, end_idx, category_command_count, 
                   categories[category_filter].name, RESET);
        }
    }
}

void execute_selected_command(const Command* command, const char* args) {
    // Handle built-in commands
    if (strcmp(command->name, "refresh") == 0) {
        fancy_print("Refreshing command list...", BLUE, 10);
        refresh_command_list();
        categorize_commands();
        load_command_descriptions();
        return;
    } else if (strcmp(command->name, "sysinfo") == 0) {
        print_system_info();
        return;
    } else if (strcmp(command->name, "clear") == 0) {
        return;  // Screen will be cleared on next loop
    } else if (strcmp(command->name, "restart") == 0) {
        fancy_print("Restarting WolfOS...", BLUE, 10);
        sleep(1);
        return;
    } else if (strncmp(command->name, "learn-", 6) == 0 || strcmp(command->name, "command-quiz") == 0) {
        launch_learning_module(command->name);
        return;
    }
    
    // External commands
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        char cmd_path[PATH_MAX];
        int found = 0;
        
        // Try to find the command in PATH
        if (access(command->name, X_OK) == 0) {
            strncpy(cmd_path, command->name, sizeof(cmd_path) - 1);
            found = 1;
        } else {
            const char *paths[] = {"/bin", "/usr/bin", "/sbin", "/usr/sbin", NULL};
            for (int p = 0; paths[p] != NULL; p++) {
                char test_path[PATH_MAX];
                snprintf(test_path, sizeof(test_path), "%s/%s", paths[p], command->name);
                
                if (access(test_path, X_OK) == 0) {
                    strncpy(cmd_path, test_path, sizeof(cmd_path) - 1);
                    found = 1;
                    break;
                }
            }
        }
        
        if (!found) {
            printf("%s%sCommand not found: %s%s\n", BOLD, RED, command->name, RESET);
            exit(1);
        }
        
        // Build argument list
        char* argv[64];
        int argc = 0;
        argv[argc++] = strdup(command->name);
        
        if (strlen(args) > 0) {
            char* args_copy = strdup(args);
            char* token = strtok(args_copy, " ");
            
            while (token != NULL && argc < 63) {
                argv[argc++] = token;
                token = strtok(NULL, " ");
            }
        }
        
        argv[argc] = NULL;
        
        execv(cmd_path, argv);
        perror("Command execution failed");
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                printf("\n%s%sCommand exited with code %d%s\n", BOLD, RED, exit_code, RESET);
            } else {
                printf("\n%s%sCommand completed successfully%s\n", BOLD, GREEN, RESET);
            }
        } else {
            printf("\n%s%sCommand terminated abnormally%s\n", BOLD, RED, RESET);
        }
    } else {
        perror("Fork failed");
    }
}

void show_command_help(const char* command) {
    strcpy(current_help_title, command);
    
    if (strcmp(command, "refresh") == 0) {
        strcpy(current_help_text, "REFRESH - WolfOS built-in command\n\n"
               "Usage: refresh\n\n"
               "Refresh the list of available commands from the system.\n"
               "This will scan the standard system directories (/bin, /usr/bin, etc.)\n"
               "for executable commands and categorize them.\n\n"
               "No arguments are needed for this command.");
        return;
    } else if (strcmp(command, "sysinfo") == 0) {
        strcpy(current_help_text, "SYSINFO - WolfOS built-in command\n\n"
               "Usage: sysinfo\n\n"
               "Display system information including:\n"
               "- Hostname\n"
               "- Kernel version\n"
               "- CPU information\n"
               "- Memory usage\n"
               "- Internet connectivity status\n\n"
               "No arguments are needed for this command.");
        return;
    } else if (strncmp(command, "learn-", 6) == 0) {
        char topic[32];
        strcpy(topic, command + 6);  // Skip the "learn-" prefix
        
        snprintf(current_help_text, MAX_HELP_TEXT_LENGTH, 
                 "LEARN-%s - WolfOS Interactive Learning Module\n\n"
                 "Usage: %s\n\n"
                 "Interactive tutorial system for learning %s.\n"
                 "This module provides step-by-step lessons and practice exercises.\n\n"
                 "The tutorial is completely interactive and will guide you through\n"
                 "with examples and tasks to help you learn.\n\n"
                 "No arguments are needed for this command.",
                 topic, command, topic);
        return;
    } else if (strcmp(command, "command-quiz") == 0) {
        strcpy(current_help_text, "COMMAND-QUIZ - WolfOS Learning Tool\n\n"
               "Usage: command-quiz [category]\n\n"
               "Interactive quiz system to test your knowledge of Linux commands.\n"
               "The quiz will present you with command descriptions and ask you\n"
               "to provide the correct command.\n\n"
               "Optional arguments:\n"
               "  category    Limit quiz to a specific category of commands\n\n"
               "Example: command-quiz Files");
        return;
    }
    
    // For system commands, try to get help from 'man' or '--help'
    char man_output[MAX_HELP_TEXT_LENGTH] = "";
    FILE *fp;
    char cmd[256];
    
    // First try the man page (summary only)
    snprintf(cmd, sizeof(cmd), "MANWIDTH=80 man %s 2>/dev/null | head -20", command);
    fp = popen(cmd, "r");
    
    if (fp != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), fp) != NULL && strlen(man_output) < MAX_HELP_TEXT_LENGTH - 256) {
            strcat(man_output, line);
        }
        pclose(fp);
    }
    
    // If man page failed, try --help
    if (strlen(man_output) < 10) {
        memset(man_output, 0, MAX_HELP_TEXT_LENGTH);
        
        snprintf(cmd, sizeof(cmd), "%s --help 2>&1 | head -20", command);
        fp = popen(cmd, "r");
        
        if (fp != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), fp) != NULL && strlen(man_output) < MAX_HELP_TEXT_LENGTH - 256) {
                strcat(man_output, line);
            }
            pclose(fp);
        }
    }
    
    // If we got some help text, use it
    if (strlen(man_output) > 10) {
        strncpy(current_help_text, man_output, MAX_HELP_TEXT_LENGTH - 1);
        current_help_text[MAX_HELP_TEXT_LENGTH - 1] = '\0';
        
        // Add note about viewing full help
        strcat(current_help_text, "\n\n[This is just a summary. For full help, use 'man ");
        strcat(current_help_text, command);
        strcat(current_help_text, "' or '");
        strcat(current_help_text, command);
        strcat(current_help_text, " --help' in your terminal.]");
    } else {
        // Generic help message if we couldn't get specific help
        snprintf(current_help_text, MAX_HELP_TEXT_LENGTH,
                 "Command: %s\n\n"
                 "No detailed help information available for this command.\n\n"
                 "Try running the command with --help flag for more information:\n"
                 "  %s --help\n\n"
                 "Or refer to the manual page:\n"
                 "  man %s",
                 command, command, command);
    }
}

void launch_learning_module(const char* topic) {
    if (strcmp(topic, "learn-bash") == 0) {
        snprintf(current_help_text, MAX_HELP_TEXT_LENGTH,
                 "%s%sBASH TUTORIAL - INTERACTIVE LEARNING MODULE%s\n\n"
                 "Welcome to the interactive Bash tutorial! This module will teach you\n"
                 "the basics of Bash shell scripting. Let's get started!\n\n"
                 "LESSON 1: BASIC COMMANDS\n\n"
                 "Bash (Bourne Again SHell) is the default shell in most Linux distributions.\n"
                 "Here are some basic commands you should know:\n\n"
                 "1. echo - Display a line of text\n"
                 "   Example: echo \"Hello, World!\"\n\n"
                 "2. cd - Change directory\n"
                 "   Example: cd /home/user/Documents\n\n"
                 "3. ls - List directory contents\n"
                 "   Example: ls -la\n\n"
                 "4. mkdir - Create a directory\n"
                 "   Example: mkdir new_folder\n\n"
                 "5. rm - Remove files or directories\n"
                 "   Example: rm file.txt\n\n"
                 "LESSON 2: VARIABLES\n\n"
                 "Variables in Bash are created by assignment and referenced using $:\n\n"
                 "name=\"John\"\n"
                 "echo \"Hello, $name!\"\n\n"
                 "LESSON 3: CONDITIONALS\n\n"
                 "Bash uses if statements for conditionals:\n\n"
                 "if [ $a -eq $b ]; then\n"
                 "    echo \"a equals b\"\n"
                 "elif [ $a -gt $b ]; then\n"
                 "    echo \"a is greater than b\"\n"
                 "else\n"
                 "    echo \"a is less than b\"\n"
                 "fi\n\n"
                 "[This is a preview of the interactive tutorial. In the full version,\n"
                 "you would be able to practice these commands in an interactive shell.]",
                 BOLD, CYAN, RESET);
    } else if (strcmp(topic, "learn-linux") == 0) {
        snprintf(current_help_text, MAX_HELP_TEXT_LENGTH,
                 "%s%sLINUX BASICS - INTERACTIVE LEARNING MODULE%s\n\n"
                 "Welcome to the Linux basics tutorial! This module will introduce you\n"
                 "to the fundamental concepts of Linux operating systems.\n\n"
                 "LESSON 1: THE LINUX FILESYSTEM\n\n"
                 "Linux organizes files in a hierarchical directory structure:\n\n"
                 "/ - Root directory\n"
                 "/bin - Essential user binaries\n"
                 "/etc - System configuration files\n"
                 "/home - User home directories\n"
                 "/usr - User programs and data\n"
                 "/var - Variable data files\n\n"
                 "LESSON 2: PERMISSIONS\n\n"
                 "Linux uses a permission system with read (r), write (w), and execute (x)\n"
                 "permissions for owner, group, and others:\n\n"
                 "chmod 755 file.txt  # Sets rwx for owner, rx for group and others\n"
                 "chown user:group file.txt  # Changes file ownership\n\n"
                 "LESSON 3: PROCESSES\n\n"
                 "Linux is a multitasking system where many processes run simultaneously:\n\n"
                 "ps aux  # List all running processes\n"
                 "top     # Dynamic real-time view of processes\n"
                 "kill    # Send signals to processes\n\n"
                 "LESSON 4: INPUT/OUTPUT REDIRECTION\n\n"
                 "> - Redirect output to a file\n"
                 "< - Take input from a file\n"
                 "| - Pipe output from one command to another\n\n"
                 "[This is a preview of the interactive tutorial. In the full version,\n"
                 "you would be able to practice these concepts with guided exercises.]",
                 BOLD, CYAN, RESET);
    } else if (strcmp(topic, "learn-vim") == 0) {
        snprintf(current_help_text, MAX_HELP_TEXT_LENGTH,
                 "%s%sVIM TUTORIAL - INTERACTIVE LEARNING MODULE%s\n\n"
                 "Welcome to the Vim tutorial! This module will teach you how to use\n"
                 "the powerful Vim text editor efficiently.\n\n"
                 "LESSON 1: MODES\n\n"
                 "Vim has different modes:\n\n"
                 "- Normal mode: For navigation and manipulation (default)\n"
                 "- Insert mode: For inserting text (press i to enter)\n"
                 "- Visual mode: For selecting text (press v to enter)\n"
                 "- Command mode: For executing commands (press : to enter)\n\n"
                 "Press Esc to return to normal mode from any other mode.\n\n"
                 "LESSON 2: BASIC NAVIGATION (in normal mode)\n\n"
                 "h - Move cursor left\n"
                 "j - Move cursor down\n"
                 "k - Move cursor up\n"
                 "l - Move cursor right\n"
                 "w - Jump to start of next word\n"
                 "b - Jump to start of previous word\n"
                 "0 - Jump to start of line\n"
                 "$ - Jump to end of line\n\n"
                 "LESSON 3: EDITING COMMANDS\n\n"
                 "i - Insert before cursor\n"
                 "a - Insert after cursor\n"
                 "o - Open new line below\n"
                 "O - Open new line above\n"
                 "x - Delete character under cursor\n"
                 "dd - Delete entire line\n"
                 "yy - Yank (copy) entire line\n"
                 "p - Paste after cursor\n\n"
                 "LESSON 4: SAVING AND QUITTING\n\n"
                 ":w - Save file\n"
                 ":q - Quit (if no changes)\n"
                 ":wq - Save and quit\n"
                 ":q! - Quit without saving\n\n"
                 "[This is a preview of the interactive tutorial. In the full version,\n"
                 "you would have a practice editor to try these commands.]",
                 BOLD, CYAN, RESET);
    } else if (strcmp(topic, "command-quiz") == 0) {
        snprintf(current_help_text, MAX_HELP_TEXT_LENGTH,
                 "%s%sCOMMAND QUIZ - TEST YOUR KNOWLEDGE%s\n\n"
                 "Welcome to the Command Quiz! This interactive tool will test your\n"
                 "knowledge of Linux commands.\n\n"
                 "Quiz Preview:\n\n"
                 "Question 1: Which command lists the contents of a directory?\n"
                 "Answer: ls\n\n"
                 "Question 2: Which command changes your current directory?\n"
                 "Answer: cd\n\n"
                 "Question 3: Which command displays the manual page for a command?\n"
                 "Answer: man\n\n"
                 "Question 4: Which command creates a new directory?\n"
                 "Answer: mkdir\n\n"
                 "Question 5: Which command removes a file?\n"
                 "Answer: rm\n\n"
                 "[This is a preview of the interactive quiz. In the full version,\n"
                 "you would be prompted to answer questions and get scored on your responses.]",
                 BOLD, CYAN, RESET);
    } else {
        // For any other command, try to create a basic learning module
        snprintf(current_help_text, MAX_HELP_TEXT_LENGTH,
                 "%s%sLEARNING MODULE: %s%s\n\n"
                 "This is a placeholder for a learning module about the '%s' command.\n\n"
                 "In a complete implementation, this would provide:\n\n"
                 "1. An introduction to the command and its purpose\n"
                 "2. Common usage patterns and examples\n"
                 "3. Important options and flags\n"
                 "4. Practice exercises\n"
                 "5. Tips and tricks for advanced usage\n\n"
                 "For now, try running 'man %s' or '%s --help' to learn more about this command.",
                 BOLD, CYAN, RESET);
    }
    
    strcpy(current_help_title, topic);
}

int get_terminal_width() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col > 0 ? w.ws_col : 80;
}

int get_terminal_height() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row > 0 ? w.ws_row : 24;
}

void handle_sigint(int sig) {
    fancy_print("\nOperation interrupted. WolfOS continues running...", RED, 10);
    signal(SIGINT, handle_sigint);
}

void display_splash() {
    printf("\n\n%s%s", BOLD, BLUE);
    printf("██╗    ██╗ ██████╗ ██╗     ███████╗ ██████╗ ███████╗\n");
    printf("██║    ██║██╔═══██╗██║     ██╔════╝██╔═══██╗██╔════╝\n");
    printf("██║ █╗ ██║██║   ██║██║     █████╗  ██║   ██║███████╗\n");
    printf("██║███╗██║██║   ██║██║     ██╔══╝  ██║   ██║╚════██║\n");
    printf("╚███╔███╔╝╚██████╔╝███████╗██║     ╚██████╔╝███████║\n");
    printf(" ╚══╝╚══╝  ╚═════╝ ╚══════╝╚═╝      ╚═════╝ ╚══════╝\n");
    printf("%s\n", RESET);
    
    printf("%s%sAdvanced Command Line Interface with Learning Tools%s\n\n", BOLD, CYAN, RESET);
}

void animate_loading(int fast_mode) {
    if (fast_mode) return;
    
    const char *frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    int frame_count = 10;
    
    printf("%s%sInitializing core components ", BOLD, BLUE);
    for (int i = 0; i < 10; i++) {
        printf("%s%s%s", BOLD, YELLOW, frames[i % frame_count]);
        fflush(stdout);
        usleep(80000);
        printf("\b");
    }
    printf("%s✓%s\n", GREEN, RESET);
}

void fancy_print(const char* text, const char* color, int delay_ms) {
    printf("%s%s", BOLD, color);
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        printf("%c", text[i]);
        fflush(stdout);
        usleep(delay_ms * 1000);
    }
    printf("%s\n", RESET);
}

void show_progress_bar(const char* text, int duration_ms) {
    int width = 30;
    int sleep_per_step = duration_ms / width;
    
    printf("%s%s%s [", BOLD, BLUE, text);
    for (int i = 0; i < width; i++) {
        printf("%s%s#%s", BOLD, CYAN, RESET);
        fflush(stdout);
        usleep(sleep_per_step * 1000);
    }
    printf("%s%s] Done!%s\n", BOLD, GREEN, RESET);
}

void print_system_info() {
    char hostname[128];
    gethostname(hostname, sizeof(hostname));
    
    printf("\n%s%sSystem Information:%s\n", BOLD, BLUE, RESET);
    printf("%s%s▶ Hostname:%s %s\n", BOLD, GREEN, RESET, hostname);
    
    FILE *fp;
    char buffer[256];
    
    // Get kernel version
    fp = popen("uname -r", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            printf("%s%s▶ Kernel:%s %s\n", BOLD, GREEN, RESET, buffer);
        }
        pclose(fp);
    }
    
    // Get CPU info
    fp = popen("grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            printf("%s%s▶ CPU:%s %s\n", BOLD, GREEN, RESET, buffer);
        }
        pclose(fp);
    }
    
    // Get memory info
    fp = popen("grep 'MemTotal' /proc/meminfo | awk '{print $2/1024\" MB\"}'", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            printf("%s%s▶ Memory:%s %s\n", BOLD, GREEN, RESET, buffer);
        }
        pclose(fp);
    }
    
    // Get disk usage
    fp = popen("df -h / | tail -1 | awk '{print $2 \" total, \" $4 \" free\"}'", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            printf("%s%s▶ Disk:%s %s\n", BOLD, GREEN, RESET, buffer);
        }
        pclose(fp);
    }
    
    // Get uptime
    fp = popen("uptime -p", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            printf("%s%s▶ Uptime:%s %s\n", BOLD, GREEN, RESET, buffer);
        }
        pclose(fp);
    }
    
    // Get current time
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("%s%s▶ Time:%s %s\n", BOLD, GREEN, RESET, buffer);
}

int check_internet_connection() {
    int result = system("ping -c 1 -W 1 8.8.8.8 > /dev/null 2>&1");
    if (result == 0) {
        printf("%s%s▶ Internet:%s Connected\n", BOLD, GREEN, RESET);
        return 1;
    } else {
        printf("%s%s▶ Internet:%s Not connected\n", BOLD, RED, RESET);
        return 0;
    }
}
