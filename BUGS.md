# CHIP-8 Emulator — Bug Report

Review scope: `src/chip8.c`, `src/memory.c`, `src/timer.c`, `src/audio.c`, `src/main.c`,
`src/input.c`, `src/graphics.c`, `src/sdl_wrapper.c`, `src/logger.c`, and headers.
No code was changed — findings only.

---

## Critical

### 1. `6XNN` (Set VX = NN) reads the wrong value and can read out of bounds
**File:** `src/chip8.c:157`
```c
case 0x6000: // 6XNN : Set VX to NN
    chip8->V[(opcode & 0x0F00) >> 8] = chip8->V[(opcode & 0x00FF)]; // BUG
```
This should set `VX = NN` (the literal immediate byte). Instead it treats `NN` as
*another register index* and copies `V[NN]` into `VX`. Since `NN` ranges 0x00–0xFF,
this reads `chip8->V[0]` through `chip8->V[255]` — but `V[]` only has 16 elements, so
most values of `NN` cause an out-of-bounds read on the `V` array (stack memory
corruption / undefined behavior).

`6XNN` is one of the most common opcodes in any CHIP-8 program (used to load
constants). This bug alone breaks essentially every ROM and risks crashes or
corrupted register state.

**Fix direction:** `chip8->V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;`

---

### 2. `8XY1` / `8XY2` / `8XY3` (OR/AND/XOR) compute VY with the wrong mask — out-of-bounds read
**File:** `src/chip8.c:175, 180, 185`
```c
chip8->V[(opcode & 0x0F00) >> 8] |= chip8->V[(opcode & 0x0F00) >> 4]; // should be 0x00F0
```
All three use `(opcode & 0x0F00) >> 4` for VY instead of `(opcode & 0x00F0) >> 4`.
`(opcode & 0x0F00) >> 4` can be as large as `0xF0` (240), indexing far past the
16-element `V` array — an out-of-bounds read, same class of bug as #1. The logical
operation is also simply wrong even when it doesn't crash, since it reads the wrong
register.

**Fix direction:** use `(opcode & 0x00F0) >> 4` for VY in all three cases.

---

### 3. `8XY4` (Add VY to VX, carry) never adds and never advances PC — infinite loop
**File:** `src/chip8.c:189-195`
```c
case 0x0004: // 8XY4: Add VY to VX, set VF on carry
    if (chip8->V[(opcode & 0x00F0) >> 4] > (0xFF - chip8->V[(opcode & 0xF00) >> 8])) {
        chip8->V[0xF] = 1;
    } else {
        chip8->V[0xF] = 0;
    }
    break; // no VX += VY, no PC += 2
```
Two problems:
- The actual addition (`VX += VY`) is never performed — only the carry flag is set.
- `chip8->PC` is never incremented, so the interpreter re-executes this exact
  instruction forever. Any ROM using `8XY4` (very common — e.g. any game using
  16-bit pointer math or accumulation) will hang the emulator permanently.

**Fix direction:** add `chip8->V[(opcode & 0x0F00) >> 8] += chip8->V[(opcode & 0x00F0) >> 4];`
and `chip8->PC += 2;`.

---

### 4. Sound timer logic is inverted, and timers are decremented at the wrong rate
**File:** `src/timer.c:6-19`, called from `src/chip8.c:84` inside `executeCycle`
```c
if (chip8->sound_timer > 0) {
    chip8->sound_timer--;
}
if (chip8->sound_timer == 0) {
    playSound();   // BUG: should be stopSound() here, and playSound() while >0
}
```
- The polarity is backwards: sound should **play while `sound_timer > 0`** and
  **stop when it reaches 0**. This code does the opposite — it calls `playSound()`
  every time the timer is at (or decays to) 0, i.e. almost all the time, and never
  calls `playSound()` while the timer is actually counting down.
- `updateTimers()` is called once per CPU instruction (`executeCycle`), not at a
  fixed 60 Hz. CHIP-8 timers are specified to decrement at 60 Hz regardless of
  instruction execution speed. Tying the decrement to instruction count makes
  every timing-dependent game (animation delays, sound duration) run at the wrong,
  ROM-dependent speed.

**Fix direction:** call `updateTimers()` from a fixed 60 Hz clock (independent of
instruction count), and swap the branch so `playSound()` fires while
`sound_timer > 0` and `stopSound()` fires when it hits 0.

---

### 5. Audio callback always writes silence
**File:** `src/audio.c:29-36`
```c
int16_t *buffer = (int16_t *)stream;
...
float sampleValue = (sin(phase) > 0.0) ? AMPLITUDE : -AMPLITUDE; // ±0.5
buffer[i] = sampleValue; // truncates 0.5 / -0.5 to (int16_t)0
```
`AMPLITUDE` is `0.5f`. Assigning a float in `(-1.0, 1.0)` directly to an `int16_t`
truncates toward zero, so `buffer[i]` is always `0`. The audio device plays, but
every sample is silence — the beep never actually sounds, independent of bug #4.

**Fix direction:** scale by the integer range, e.g.
`buffer[i] = (int16_t)(sampleValue * INT16_MAX);`.

---

## High

### 6. Two entire opcodes are unimplemented: `9XY0` and `BNNN`
**File:** `src/chip8.c:100` (outer `switch (opcode & 0xF000)`)
The switch handles `0x0/1/2/3/4/5/6/7/8/A/C/D/E/F` but has no `case 0x9000:` (skip
if `VX != VY`) and no `case 0xB000:` (jump to `NNN + V0`). Both fall through to the
`default` handler:
```c
default:
    printf("Unknown opcode: 0x%X\n", opcode);
    exit(1);
```
Any ROM that uses either opcode — both are common and part of the base CHIP-8
instruction set — terminates the entire emulator process.

---

### 7. `8XY5` borrow check compares the wrong register due to operator precedence
**File:** `src/chip8.c:199`
```c
if (chip8->V[(opcode & 0x00F0) >> 4] > chip8->V[(opcode & 0x0F00 >> 8)]) {
```
`>>` binds tighter than `&` in C, so `(opcode & 0x0F00 >> 8)` evaluates as
`opcode & (0x0F00 >> 8)` = `opcode & 0x0F`, **not** `(opcode & 0x0F00) >> 8`. The
borrow-flag (`VF`) comparison reads a garbage/wrong register while the actual
subtraction two lines below (`chip8->V[(opcode & 0x0F00) >> 8] -= ...`) uses the
correct index. Result: `VF` is set incorrectly after `8XY5`, breaking any game logic
that depends on the borrow flag.

**Fix direction:** `chip8->V[(opcode & 0x0F00) >> 8]` (matching the line below).

---

### 8. Stack pointer has no overflow/underflow checks
**File:** `src/chip8.c:111-115` (`00EE` return), `127-131` (`2NNN` call)
- `2NNN`: `chip8->stack[chip8->SP]` is written and `SP` incremented with no check
  against `CHIP8_STACK_SIZE` (16). A ROM (or corrupted PC from another bug) that
  calls more than 16 subroutines deep silently overflows the 16-entry stack array.
- `00EE`: `chip8->SP--` with no check that `SP > 0`. A stray/malformed `RET` with an
  empty call stack underflows `SP` (wraps to 255 as `uint8_t`) and then reads
  `stack[255]`, far out of bounds of the 16-element array.

Both are undefined behavior / memory corruption on malformed or adversarial ROMs,
not just a "shouldn't happen" edge case for an interpreter meant to run untrusted
ROM files.

---

## Medium

### 9. `fetchOpcode` has no bounds check on `PC`
**File:** `src/chip8.c:77-79`
```c
uint16_t fetchOpcode(chip8_t *chip8) {
    return (chip8->memory[chip8->PC] << 8) | chip8->memory[chip8->PC + 1];
}
```
Nothing clamps `PC` to a valid range before this read. If `PC` reaches `0xFFF` (the
last valid byte), `memory[chip8->PC + 1]` reads `memory[0x1000]`, one past the end of
the 4096-byte array. Combined with bug #1/#2 corrupting `PC`-adjacent state, or a ROM
that jumps to the top of memory, this is a real out-of-bounds read path, not just
theoretical.

### 10. `loadROM` doesn't validate `fseek`/`ftell`/`fread` results
**File:** `src/memory.c:39-62`
`ftell()` can return `-1` on failure. The size check
(`romSize > (CHIP8_MEMORY_SIZE - CHIP8_START_ADDRESS)`) happens to still reject that
case since a negative value compared against a positive bound passes the check the
"wrong way" but ends up erroring out anyway — but that's incidental, not intentional
handling, and fragile if the comparison types ever change. More importantly, the
return value of `fread()` is never checked against `romSize`, so a short/truncated
read is silently treated as success and the emulator runs on partially-loaded,
effectively garbage memory beyond the ROM's true content.

---

## Low

### 11. `isKeyPressed` is declared but never implemented
**File:** `include/chip8.h:65,67`
Declared twice (also a harmless duplicate declaration) but has no definition in any
`.c` file. Currently unused, so it doesn't break the build — but if anything calls
it, it will fail to link.

### 12. `initializeMemory()` is dead code
**File:** `src/memory.c:28-37`
Duplicates the fontset-loading logic already done by `initializeCPU()`
(`src/chip8.c:52-74`, including its own separate copy of `chip8_fontset`). Never
called from `main.c`. Two independent copies of the fontset data is also a
maintenance hazard (a fix to one won't propagate to the other).

### 13. `drawSprite` wraps every pixel instead of clipping at screen edges
**File:** `src/chip8.c:375`
```c
int displayIndex = (x + col) % CHIP8_DISPLAY_WIDTH + ((y + row) % CHIP8_DISPLAY_HEIGHT) * CHIP8_DISPLAY_WIDTH;
```
The original COSMAC VIP behavior (and most CHIP-8 reference implementations) clips
sprite pixels that go past the right/bottom edge rather than wrapping them to the
opposite side. This is a spec-fidelity issue, not a crash — some ROMs render
incorrectly (sprite pixels appear on the wrong edge) but nothing is unsafe.

### 14. `waitForKeyPress` is a second, entirely unused input-handling path
**File:** `src/chip8.c:398-436`
A 39-line function duplicating the key-mapping table already in `src/input.c`
(`handleInput`), including its own blocking `while(true)` polling loop. Nothing
calls it — `FX0A` implements its own non-blocking wait inline instead
(`src/chip8.c:292-309`). Two independent copies of the same keymap is a maintenance
hazard, same class of issue as #12.

### 15. Dead/unreachable code
- `src/chip8.c:207` — a stray extra `break;` right after the `0x0005` case's real
  `break;` (harmless, but signals a leftover edit).
- `src/audio.c:72-74` — `logInfo("Audio device OPENED!")` sits after `return 0;` and
  never executes.

---

## Summary

| # | Severity | Issue |
|---|----------|-------|
| 1 | Critical | `6XNN` reads wrong/out-of-bounds register instead of literal |
| 2 | Critical | `8XY1/2/3` compute VY with wrong mask → out-of-bounds read |
| 3 | Critical | `8XY4` never adds, never advances PC → infinite loop |
| 4 | Critical | Sound timer polarity inverted + decremented at wrong rate |
| 5 | Critical | Audio callback truncates samples to always output silence |
| 6 | High | `9XY0` and `BNNN` unimplemented → emulator exits on use |
| 7 | High | `8XY5` borrow check uses wrong register (operator precedence) |
| 8 | High | Stack has no overflow/underflow checks |
| 9 | Medium | `fetchOpcode` has no bounds check on PC |
| 10 | Medium | `loadROM` doesn't validate `fread` result |
| 11 | Low | `isKeyPressed` declared, never implemented |
| 12 | Low | `initializeMemory()` is dead/duplicated code |
| 13 | Low | Sprite drawing wraps instead of clipping at edges |
| 14 | Low | `waitForKeyPress` is dead, duplicates `input.c`'s keymap |
| 15 | Low | Minor dead/unreachable code |
