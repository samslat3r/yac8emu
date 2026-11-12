#include "timer.h"
#include "chip8.h"
#include "audio.h"

void updateTimers(chip8_t *chip8) {
	if (chip8->delay_timer > 0) {
		chip8->delay_timer--;
	}
	if (chip8->sound_timer > 0) {
		chip8->sound_timer--;
	}
	if (chip8->sound_timer > 0) {
		playSound();
	} else {
		stopSound();
	}
}
