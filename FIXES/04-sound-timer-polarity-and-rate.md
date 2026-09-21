# Fix 04: Sound timer played at the wrong times and ticked at the wrong rate

**Severity:** Critical
**Files:** `src/timer.c`, `src/chip8.c`, `src/main.c`

This bug actually had two separate problems living in the same area: wrong
*logic* (when to play sound) and wrong *timing* (how often the timer ticks).
Both needed fixing for sound/delay timers to behave correctly.

## Problem A — inverted play/stop logic

### What was wrong

```c
// src/timer.c (before)
if (chip8->sound_timer > 0) {
    chip8->sound_timer--;
}
if (chip8->sound_timer == 0) {
    playSound();
}
```

The CHIP-8 spec says: while `sound_timer > 0`, the buzzer should be sounding;
once it reaches `0`, it should stop. The code did the opposite — it called
`playSound()` every time the timer *was already at* (or just decayed to)
zero, and never called anything while the timer was actually still counting
down. In practice this meant `playSound()` fired almost constantly (whenever
the timer is idle at 0) and the game's actual "beep for N ticks" requests
were ignored.

### The fix

```c
// src/timer.c (after)
if (chip8->sound_timer > 0) {
    chip8->sound_timer--;
}
if (chip8->sound_timer > 0) {
    playSound();
} else {
    stopSound();
}
```

Now: decrement first (as before), then check `> 0` (not `== 0`) to decide
whether to play. If the timer is at `0`, `stopSound()` is called instead of
`playSound()`. Both `playSound()` and `stopSound()` were already implemented
correctly in `audio.c` — they were just never being called in the right
place.

## Problem B — timer decremented once per instruction instead of at 60Hz

### What was wrong

```c
// src/chip8.c (before), inside executeCycle()
void executeCycle(chip8_t *chip8) {
    chip8->opcode = fetchOpcode(chip8);
    decodeAndExecute(chip8, chip8->opcode);
    updateTimers(chip8);
}
```

`executeCycle()` runs once per emulated CPU instruction, and `main.c`'s loop
calls it as fast as the ~2ms delay allows — hundreds of times per second, and
that rate isn't fixed either (it depends on how long rendering/input take).
The CHIP-8 spec requires the delay and sound timers to decrement at a fixed
**60Hz**, completely independent of how many instructions the CPU executes.
By calling `updateTimers()` once per instruction, every timing-dependent
behavior (animation delays via the delay timer, beep duration via the sound
timer) ran at a speed tied to instruction throughput instead of real time —
different ROMs (or different machines) would see wildly different delay/beep
durations.

### The fix

`updateTimers()` was removed from `executeCycle()` entirely:

```c
// src/chip8.c (after)
void executeCycle(chip8_t *chip8) {
    chip8->opcode = fetchOpcode(chip8);
    decodeAndExecute(chip8, chip8->opcode);
    // Timers are NOT updated here: they must tick at a fixed 60Hz, not once
    // per instruction. See updateTimers() calls in main.c's loop.
}
```

And a fixed-rate call was added to the main loop in `main.c`, gated by real
elapsed wall-clock time (`SDL_GetTicks()`), separate from the instruction
throttling that already existed:

```c
// src/main.c (after)
#define TIMER_INTERVAL_MS (1000 / 60)
...
uint32_t lastTimerTime = SDL_GetTicks();
while (running) {
    handleInput(&chip8, &running);
    executeCycle(&chip8);

    if (chip8.drawFlag) {
        renderGraphics(&chip8);
        chip8.drawFlag = false;
    }

    // Delay/sound timers tick at a fixed 60Hz, independent of how many
    // instructions ran in between.
    uint32_t nowForTimer = SDL_GetTicks();
    if (nowForTimer - lastTimerTime >= TIMER_INTERVAL_MS) {
        updateTimers(&chip8);
        lastTimerTime = nowForTimer;
    }

    ...
}
```

`main.c` now needs `timer.h`, which was added to its includes.

## Lesson for next time

Anything documented as running "at Nhz" is a real-time rate, not a
per-iteration rate. Never tie a fixed-frequency behavior (timers, physics
steps, network heartbeats) to how often an unrelated loop happens to run —
instead check elapsed wall-clock time (`SDL_GetTicks()`, or an equivalent) and
only fire when enough real time has passed.

Also: an `if (x == 0)` and an `if (x > 0)` on the same variable are not
"basically the same check with `else` swapped" — when there's a third branch
implied (do nothing vs. call the opposite function), get the exact condition
right, since these two conditions overlap at every non-zero-to-zero
transition but diverge everywhere else.

## How to verify

Set `sound_timer = 30` (half a second at 60Hz) and step the emulator forward.
Confirm `playSound()` fires (via `isSoundPlaying()` or a log line) while the
timer counts down from 30 to 1, and `stopSound()` fires once it hits 0 — and
confirm this takes roughly 500ms of wall-clock time regardless of how fast
the ROM's instructions execute.
