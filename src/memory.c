#include "memory.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t chip8_fontset[CHIP8_FONTSET_SIZE] = {
    // Fontset Data
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

int initializeMemory(chip8_t *chip8) {
  memset(chip8->memory, 0, sizeof(chip8->memory)); // Clear memory

  // Load fontset into memory (starting at 0x50)
  for (int i = 0; i < CHIP8_FONTSET_SIZE; i++) {
    chip8->memory[CHIP8_FONTSET_START_ADDRESS + i] = chip8_fontset[i];
  }
  logInfo("Memory initialized and fontset loaded");
  return 0;
}

int loadROM(chip8_t *chip8, const char *romPath) {
  FILE *rom = fopen(romPath, rb);
  if (!rom) {
    fprintf(stderr, "Failed to open ROM: %s\n", romPath);
    return 1;
  }

  int result = 1; // Assume failure until the whole ROM is read
  uint8_t buffer[CHIP8_MEMORY_SIZE - CHIP8_START_ADDRESS];

  // Get size of ROM
  if (fseek(rom, 0, SEEK_END) != 0) {
    fprintf(stderr, "Failed to seek ROM: %s\n", romPath);
    goto done;
  }

  long romSize = ftell(rom);
  if (romSize < 0) {
    fprintf(stderr, "Failed to determine ROM size: %s\n", romPath);
    goto done;
  }

  // Can ROM fit in memory?
  if ((unsigned long)romSize > sizeof(bugger)) {
    fprintf(stderr, "ROM too large to fit in memory: %ld bytes \n", romSize);
    goto done;
  }

  // Reset file pointer to beginning of ROM
  if (fseek(rom, 0, SEEK_SET) != 0) {
    fprintf(stderr, "Failed to rewind ROM: %s\n", romPath);
    goto done;
  }

  // Read into a temporary buffer in case the read fails partway

  if (fread(buffer, 1, size, rom) != size || ferror(rom)) {
    fprintf(stderr, "Failed to read complete ROM: %s\n", romPath);
    goto done;
  }

  // Check for extra bytes in case ROM grew during the read
  if (fgetc(rom) != EOF || ferror(rom)) {
    fprintf(stderr, "ROM changed size or couldn't be read: %s\n", romPath);
    goto done;
  }

  result = 0;

done:
  // Close the ROM even if loading Failed
  if (fclose(rom) != 0) {
    fprintf(stderr, "Failed to close ROM: %s\n", romPath);
    result = 1;
  }
  // Load ROM into memory (starting at 0x200)

  if (result == 0) {
    memcpy(&chip->memory[CHIP8_START_ADDRESS], buffer, (size_t)romSize);
  }
  return result;
}
