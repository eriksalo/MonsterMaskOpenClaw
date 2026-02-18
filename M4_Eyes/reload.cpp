// SPDX-FileCopyrightText: 2024 Monster Mask Open Claw Project
//
// SPDX-License-Identifier: MIT

// Runtime eye configuration reload with texture caching.
// Allows switching between mood config files without rebooting.
// Geometry (eyeRadius, irisRadius, slitPupilRadius) is kept fixed
// to avoid regenerating the ~125KB polar lookup tables.

#include "globals.h"
#include <string.h>

extern Adafruit_Arcada arcada;
extern SPISettings     settings;

// Texture cache — avoids re-writing to internal flash (finite resource)
#define MAX_CACHED_TEXTURES 8
struct TextureCacheEntry {
  char     filename[48];
  uint16_t *data;
  uint16_t  width;
  uint16_t  height;
};
static TextureCacheEntry textureCache[MAX_CACHED_TEXTURES];
static uint8_t           numCached = 0;

// Current mood name for STATUS reporting
char currentMoodName[16] = "default";

// Look up a texture in cache by filename. Returns pointer to entry or NULL.
static TextureCacheEntry *findCachedTexture(const char *filename) {
  for (uint8_t i = 0; i < numCached; i++) {
    if (!strcmp(textureCache[i].filename, filename)) {
      return &textureCache[i];
    }
  }
  return NULL;
}

// Add a texture to the cache after loading.
static void addCachedTexture(const char *filename, uint16_t *data,
                             uint16_t width, uint16_t height) {
  if (numCached >= MAX_CACHED_TEXTURES) {
    Serial.println("RELOAD: Texture cache full, not caching");
    return;
  }
  TextureCacheEntry *entry = &textureCache[numCached++];
  strncpy(entry->filename, filename, sizeof(entry->filename) - 1);
  entry->filename[sizeof(entry->filename) - 1] = '\0';
  entry->data   = data;
  entry->width  = width;
  entry->height = height;
}

// Load a texture, using cache if available.
// Returns IMAGE_SUCCESS on success. On failure, sets data/width/height to
// the fallback color pointer.
static ImageReturnCode loadTextureWithCache(char *filename, uint16_t **data,
                                            uint16_t *width, uint16_t *height,
                                            uint16_t *fallbackColor,
                                            uint32_t maxRam) {
  if (filename == NULL) {
    *data   = fallbackColor;
    *width  = 1;
    *height = 1;
    return IMAGE_ERR_FILE_NOT_FOUND;
  }

  TextureCacheEntry *cached = findCachedTexture(filename);
  if (cached) {
    Serial.printf("RELOAD: Texture cache hit: %s\n", filename);
    *data   = cached->data;
    *width  = cached->width;
    *height = cached->height;
    return IMAGE_SUCCESS;
  }

  Serial.printf("RELOAD: Loading texture: %s\n", filename);
  ImageReturnCode status = loadTexture(filename, data, width, height, maxRam);
  if (status == IMAGE_SUCCESS) {
    addCachedTexture(filename, *data, *width, *height);
  } else {
    Serial.printf("RELOAD: Texture load failed: %s\n", filename);
    *data   = fallbackColor;
    *width  = 1;
    *height = 1;
  }
  return status;
}

void initReloadState(void) {
  reloadRequested = false;
  reloadConfigPath[0] = '\0';

  // Seed the texture cache with whatever was loaded at boot
  for (uint8_t e = 0; e < NUM_EYES; e++) {
    // We don't have filenames anymore (they were freed in setup),
    // so we can't seed the cache retroactively. That's OK — the first
    // mood switch will populate the cache.
  }
  Serial.println("RELOAD: Mood reload system initialized");
}

void reloadEyeConfig(const char *configPath) {
  uint8_t e;
  uint32_t timeout;

  Serial.printf("RELOAD: Starting reload with config: %s\n", configPath);

  // 1. Wait for all eyes' DMA to finish
  for (e = 0; e < NUM_EYES; e++) {
    timeout = millis();
    while (eye[e].dma_busy) {
      if ((millis() - timeout) > 100) {
        Serial.printf("RELOAD: DMA timeout on eye %d, forcing\n", e);
        eye[e].dma.fix();
        eye[e].dma_busy = false;
        break;
      }
    }
  }

  // 2. End SPI transactions on both eyes
  for (e = 0; e < NUM_EYES; e++) {
    digitalWrite(eye[e].cs, HIGH);  // Deselect
    eye[e].spi->endTransaction();
  }

  // 3. Free old dynamic allocations (eyelid filenames, texture filenames)
  // Note: texture data lives in flash and cannot be freed — that's what
  // the cache is for. Eyelid filenames and texture filenames from the
  // previous loadConfig are already freed in the original setup flow,
  // but loadConfig will strdup new ones.
  // We need to free any filenames that loadConfig will allocate.
  if (upperEyelidFilename) { free(upperEyelidFilename); upperEyelidFilename = NULL; }
  if (lowerEyelidFilename) { free(lowerEyelidFilename); lowerEyelidFilename = NULL; }
  for (e = 0; e < NUM_EYES; e++) {
    if (eye[e].iris.filename)   { free(eye[e].iris.filename);   eye[e].iris.filename   = NULL; }
    if (eye[e].sclera.filename) { free(eye[e].sclera.filename); eye[e].sclera.filename = NULL; }
  }

  // 4. Reset eye struct defaults (mirrors setup() lines 235-258)
  //    DO NOT touch: name, spi, cs, dc, rst, winkPin, column[], display,
  //    dma, dptr (hardware config). DO NOT touch colNum/colIdx/dma_busy/
  //    column_ready yet — we reset those at the end.
  for (e = 0; e < NUM_EYES; e++) {
    eye[e].pupilColor        = 0x0000;
    eye[e].backColor         = 0xFFFF;
    eye[e].iris.color        = 0xFF01;
    eye[e].iris.data         = NULL;
    eye[e].iris.filename     = NULL;
    eye[e].iris.startAngle   = (e & 1) ? 512 : 0;
    eye[e].iris.angle        = eye[e].iris.startAngle;
    eye[e].iris.mirror       = 0;
    eye[e].iris.spin         = 0.0;
    eye[e].iris.iSpin        = 0;
    eye[e].sclera.color      = 0xFFFF;
    eye[e].sclera.data       = NULL;
    eye[e].sclera.filename   = NULL;
    eye[e].sclera.startAngle = (e & 1) ? 512 : 0;
    eye[e].sclera.angle      = eye[e].sclera.startAngle;
    eye[e].sclera.mirror     = 0;
    eye[e].sclera.spin       = 0.0;
    eye[e].sclera.iSpin      = 0;
    eye[e].rotation          = 3;
    eye[e].blink.state       = NOBLINK;
    eye[e].blinkFactor       = 0.0;
  }

  // Reset globals that loadConfig sets
  tracking    = true;
  trackFactor = 0.5;
  gazeMax     = 3000000;
  irisMin     = 0.45;
  irisRange   = 0.35;

  // 5. Load new config (preserves eyeRadius/irisRadius/slitPupilRadius geometry)
  //    Save geometry before loadConfig overwrites it
  int savedEyeRadius       = eyeRadius;
  int savedEyeDiameter     = eyeDiameter;
  int savedIrisRadius      = irisRadius;
  int savedSlitPupilRadius = slitPupilRadius;
  int savedMapRadius       = mapRadius;
  int savedMapDiameter     = mapDiameter;
  float savedCoverage      = coverage;

  loadConfig((char *)configPath);

  // Restore fixed geometry — do NOT allow config to change these
  eyeRadius       = savedEyeRadius;
  eyeDiameter     = savedEyeDiameter;
  irisRadius      = savedIrisRadius;
  slitPupilRadius = savedSlitPupilRadius;
  mapRadius       = savedMapRadius;
  mapDiameter     = savedMapDiameter;
  coverage        = savedCoverage;

  Serial.println("RELOAD: Config loaded, loading textures...");

  // 6. Load textures with cache
  uint32_t maxRam = availableRAM() - stackReserve;
  uint8_t e2;

  for (e = 0; e < NUM_EYES; e++) {
    yield();
    // Check if this eye shares iris texture with a prior eye
    bool shared = false;
    for (e2 = 0; e2 < e; e2++) {
      if ((eye[e].iris.filename && eye[e2].iris.filename) &&
          (!strcmp(eye[e].iris.filename, eye[e2].iris.filename))) {
        eye[e].iris.data   = eye[e2].iris.data;
        eye[e].iris.width  = eye[e2].iris.width;
        eye[e].iris.height = eye[e2].iris.height;
        shared = true;
        break;
      }
    }
    if (!shared) {
      loadTextureWithCache(eye[e].iris.filename, &eye[e].iris.data,
                           &eye[e].iris.width, &eye[e].iris.height,
                           &eye[e].iris.color, maxRam);
    }

    // Repeat for sclera
    shared = false;
    for (e2 = 0; e2 < e; e2++) {
      if ((eye[e].sclera.filename && eye[e2].sclera.filename) &&
          (!strcmp(eye[e].sclera.filename, eye[e2].sclera.filename))) {
        eye[e].sclera.data   = eye[e2].sclera.data;
        eye[e].sclera.width  = eye[e2].sclera.width;
        eye[e].sclera.height = eye[e2].sclera.height;
        shared = true;
        break;
      }
    }
    if (!shared) {
      loadTextureWithCache(eye[e].sclera.filename, &eye[e].sclera.data,
                           &eye[e].sclera.width, &eye[e].sclera.height,
                           &eye[e].sclera.color, maxRam);
    }
  }

  // 7. Load eyelids
  Serial.println("RELOAD: Loading eyelids...");
  yield();
  loadEyelid(upperEyelidFilename ?
    upperEyelidFilename : (char *)"upper.bmp",
    upperClosed, upperOpen, DISPLAY_SIZE - 1, maxRam);
  loadEyelid(lowerEyelidFilename ?
    lowerEyelidFilename : (char *)"lower.bmp",
    lowerOpen, lowerClosed, 0, maxRam);

  // 8. Free temporary filenames
  for (e = 0; e < NUM_EYES; e++) {
    if (eye[e].sclera.filename) { free(eye[e].sclera.filename); eye[e].sclera.filename = NULL; }
    if (eye[e].iris.filename)   { free(eye[e].iris.filename);   eye[e].iris.filename   = NULL; }
  }
  if (lowerEyelidFilename) { free(lowerEyelidFilename); lowerEyelidFilename = NULL; }
  if (upperEyelidFilename) { free(upperEyelidFilename); upperEyelidFilename = NULL; }

  // 9. Reset rendering state
  for (e = 0; e < NUM_EYES; e++) {
    eye[e].colNum       = DISPLAY_SIZE; // Force wraparound to first column
    eye[e].colIdx       = 0;
    eye[e].dma_busy     = false;
    eye[e].column_ready = false;
    eye[e].eyeX         = mapRadius;
    eye[e].eyeY         = mapRadius;
    eye[e].display->setRotation(eye[e].rotation);
  }

  Serial.printf("RELOAD: Complete! Free RAM: %d\n", availableRAM());
}
