#ifndef VERSION_H
#define VERSION_H

// Firmware version information
// Format: YYYYMMDD-branch-commitid
// Example: 20251202-main-a1b2c3

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "unknown"
#endif

#ifndef BUILD_DATE
#define BUILD_DATE __DATE__
#endif

#ifndef BUILD_TIME
#define BUILD_TIME __TIME__
#endif

#ifndef GIT_BRANCH
#define GIT_BRANCH "unknown"
#endif

#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif

// Get full version string
const char* get_firmware_version(void);

#endif // VERSION_H
