// SPDX-FileCopyrightText: 2024 Monster Mask Open Claw Project
//
// SPDX-License-Identifier: MIT

// Serial mood controller for Monster M4SK animated eyes.
// Accepts newline-terminated commands over Serial at 115200 baud.
// Protocol designed for easy relay from MQTT via ESP32 bridge.
//
// Commands:
//   MOOD:<name>   Switch to named mood (happy|angry|sad|scared|sleepy|
//                 suspicious|surprised|love|crazy)
//   MOOD:list     List available moods
//   STATUS        Print current mood and frame info

#if 1 // Change to 0 to disable this code (must enable ONE user*.cpp only!)

#include "globals.h"
#include <string.h>

extern char     currentMoodName[16]; // Defined in reload.cpp
extern uint32_t frames;              // Defined in M4_Eyes.ino

// Mood name to config path mapping
struct MoodEntry {
  const char *name;
  const char *configPath;
};

static const MoodEntry moodTable[] = {
  { "happy",      "moods/happy/config.eye"      },
  { "angry",      "moods/angry/config.eye"      },
  { "sad",        "moods/sad/config.eye"        },
  { "scared",     "moods/scared/config.eye"     },
  { "sleepy",     "moods/sleepy/config.eye"     },
  { "suspicious", "moods/suspicious/config.eye" },
  { "surprised",  "moods/surprised/config.eye"  },
  { "love",       "moods/love/config.eye"       },
  { "crazy",      "moods/crazy/config.eye"      },
};
static const uint8_t NUM_MOODS = sizeof(moodTable) / sizeof(moodTable[0]);

// Serial input buffer
static char    serialBuf[64];
static uint8_t serialIdx = 0;

// Process a complete command line
static void processCommand(const char *cmd) {
  // Skip leading whitespace
  while (*cmd == ' ' || *cmd == '\t') cmd++;

  if (!strncasecmp(cmd, "MOOD:", 5)) {
    const char *moodName = cmd + 5;

    // Handle list command
    if (!strcasecmp(moodName, "list")) {
      Serial.println("MOOD:LIST");
      for (uint8_t i = 0; i < NUM_MOODS; i++) {
        Serial.printf("  %s -> %s\n", moodTable[i].name, moodTable[i].configPath);
      }
      Serial.printf("MOOD:CURRENT:%s\n", currentMoodName);
      return;
    }

    // Look up mood name
    for (uint8_t i = 0; i < NUM_MOODS; i++) {
      if (!strcasecmp(moodName, moodTable[i].name)) {
        // Set up reload request
        strncpy(reloadConfigPath, moodTable[i].configPath, sizeof(reloadConfigPath) - 1);
        reloadConfigPath[sizeof(reloadConfigPath) - 1] = '\0';
        strncpy(currentMoodName, moodTable[i].name, sizeof(currentMoodName) - 1);
        currentMoodName[sizeof(currentMoodName) - 1] = '\0';
        Serial.printf("MOOD:SWITCHING:%s\n", moodTable[i].name);
        reloadRequested = true;
        return;
      }
    }

    Serial.printf("UNKNOWN:MOOD:%s\n", moodName);

  } else if (!strncasecmp(cmd, "STATUS", 6)) {
    Serial.printf("STATUS:mood=%s,frames=%lu,freeRAM=%lu\n",
                  currentMoodName, (unsigned long)frames, (unsigned long)availableRAM());

  } else if (cmd[0] != '\0') {
    Serial.printf("UNKNOWN:CMD:%s\n", cmd);
  }
}

void user_setup(void) {
  // Serial is already initialized in M4_Eyes.ino setup() at 115200
  Serial.println("Serial mood controller ready");
  Serial.println("Commands: MOOD:<name>, MOOD:list, STATUS");
}

void user_loop(void) {
  // Non-blocking serial read
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialIdx > 0) {
        serialBuf[serialIdx] = '\0';
        processCommand(serialBuf);
        serialIdx = 0;
      }
    } else if (serialIdx < sizeof(serialBuf) - 1) {
      serialBuf[serialIdx++] = c;
    }
    // If buffer overflows, just keep overwriting last char
  }
}

#endif // 1
