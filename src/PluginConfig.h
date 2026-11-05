#pragma once

// Identity of the plug-in. Keep the FUIDs as they are: hosts use them to find the plug-in and restore
// saved projects.

#define DIFFUSIONDELAY_NAME "DiffusionDelay"
#define DIFFUSIONDELAY_VENDOR "Hadronyche"
#define DIFFUSIONDELAY_URL ""
#define DIFFUSIONDELAY_EMAIL ""

// Class IDs, passed to Steinberg::FUID (a, b, c, d)
#define DIFFUSIONDELAY_PROCESSOR_UID 0x75D5B919, 0x9B1B4F84, 0xAA4EA7BA, 0xF77997FD
#define DIFFUSIONDELAY_CONTROLLER_UID 0x29D38EDE, 0x8A5A47D6, 0xAC09247F, 0x94AEB704
