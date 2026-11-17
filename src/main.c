#include "chip8.h"
#include "graphics.h"
#include "input.h"
#include "audio.h"
#include "memory.h"
#include "logger.h"
#include "timer.h"
#include "sdl_wrapper.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

void cleanup() {	
	destroyGraphics();
	cleanupAudio();
	destroySDL();
	closeLogger();
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s <ROM_FILE>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	// Init logger, SDL, Graphics, Audio, CHIP-8 instance

	initLogger("logs/chip8_emulator.log");
	logInfo("CHIP-8 Emulator started");

	if (initializeSDL() != 0) {
		logError("Failed to initialize SDL");
		closeLogger();
		return EXIT_FAILURE;
	}

	if (initializeGraphics() != 0) {
		logError("Failed to initialize graphics");
		destroySDL();
		closeLogger();
		return EXIT_FAILURE;
	}

	if (initializeAudio() != 0) {
		logError("Failed to initialize audio");
		destroyGraphics();
		destroySDL();
		closeLogger();
		return EXIT_FAILURE;
	}

	//Create CHIP8 instance
	chip8_t chip8;
	initializeCPU(&chip8);

	// Load ROM
	if (loadROM(&chip8, argv[1]) != 0) {
		logError("Failed to load ROM");
		cleanup();
		return EXIT_FAILURE;
	}
		
	// Main emulation loop
	
	bool running = true;
	const double cpuPeriod = 1.0 / 500.0;
	const double timerPeriod = 1.0 / 60.0;
	const Uint64 frequency = SDL_GetPerformanceFrequency();
	Uint64 previous = DFL_GetPerformanceCounter();
	double cpuElapsed = 0.0;
	double timerElapsed = 0.0;

	while (running) {
		handleInput(&chip8, &running);
		if (!running) {
			break;
		}
	
		Uint64 now = SDL_GetPerformanceCounter();
		double elapsed = (double)(now - previous) / (double)frequency;
		previous = now;
		cpuElapsed += elapsed;
		timerElapsed += elapsed;

		while (timerElapsed >= timerPeriod) {
			updateTimers(&chip8);
			timerElapsed -= timerPeriod;
		}
	
		while (cpuElapsed >= cpuPeriod) {
			executeCycle(&chip8);
			cpuElapsed -= cpuPeriod;
		}
	
		if (chip8.drawFlag) {
			renderGraphics(&chip8);
			chip8.drawFlag = false;
		}
		SDL_Delay(1);	
	}

	//Cleanup before exiting
	cleanup();

	logInfo("CHIP-8 Emulator was nicely terminated");
	return EXIT_SUCCESS;
}
