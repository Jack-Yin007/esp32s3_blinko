#include "version.h"
#include <stdio.h>
#include <string.h>

static char version_string[64] = {0};

const char* get_firmware_version(void) {
    if (version_string[0] == '\0') {
        // Format: YYYYMMDD-branch-commit
        snprintf(version_string, sizeof(version_string), 
                 "%s-%s-%s", 
                 BUILD_DATE, 
                 GIT_BRANCH, 
                 GIT_COMMIT);
    }
    return version_string;
}
