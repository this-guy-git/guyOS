#include <stdint.h>
#include <stdbool.h>
#include "../include/fat.h"

bool fs_initialize_structure(void) {
    // Create main directories
    const char *dirs[] = {
        "usr",
        "shell",
        "sys",
    };
    
    for (int i = 0; i < 3; i++) {
        if (!fat_mkdir(dirs[i])) {
            // Directory might already exist, which is OK
        }
    }
    
    return true;
}

bool fs_create_user_directory(const char *username) {
    // For now, create user directories in root
    // Later implement subdirectory navigation
    // and create them in /usr/username
    return fat_mkdir(username);
}