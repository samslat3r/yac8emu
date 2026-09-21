# Fix 05: Audio callback truncated every sample to silence

**Severity:** Critical
**File:** `src/audio.c`
**Function:** `audioCallback()`

## What was wrong

```c
#define AMPLITUDE 0.5f
...
int16_t *buffer = (int16_t *)stream;
...
float sampleValue = (sin(phase) > 0.0) ? AMPLITUDE : -AMPLITUDE; // +0.5 or -0.5
buffer[i] = sampleValue; // BUG
```

The audio device was configured for 16-bit signed integer samples
(`AUDIO_S16SYS`), so `buffer` is an `int16_t*` and each sample needs to be a
whole number somewhere in the range `-32768` to `32767`.

`sampleValue` is a `float` holding either `+0.5` or `-0.5` (a square wave
between `-AMPLITUDE` and `+AMPLITUDE`). Assigning a `float` directly to an
`int16_t` in C **truncates toward zero** — it does not scale the value into
the integer's range. `0.5` truncates to `0`, and `-0.5` also truncates to
`0`. So every single sample written to the buffer was `0`.

## Why it mattered

Even after fixing the sound-timer logic (Fix 04) so that `playSound()` fires
at the right time, the audio device would have kept "playing" — just a
stream of pure silence. `isPlaying` becomes `true`, `SDL_PauseAudioDevice`
correctly unpauses the device, but the actual waveform being output is flat
zero. The bug is completely invisible without inspecting the sample values
themselves, since nothing crashes or logs an error — the beep is just
inaudible.

## The fix

```c
float sampleValue = (sin(phase) > 0.0) ? AMPLITUDE : -AMPLITUDE;
buffer[i] = (int16_t)(sampleValue * INT16_MAX);
```

`sampleValue` (a fraction of the maximum volume, e.g. `0.5` for 50% volume)
is multiplied by `INT16_MAX` (`32767`, the largest value an `int16_t` can
hold) *before* being cast to `int16_t`. This correctly scales the `-1.0..1.0`
style amplitude range into the full 16-bit sample range, e.g. `0.5 * 32767 ≈
16383`, which is a real, audible sample value instead of `0`.

`INT16_MAX` was already available via the existing `#include <limits.h>` at
the top of the file, so no new includes were needed.

## Lesson for next time

Floating-point audio samples are almost always expressed as a fraction of
full scale (`-1.0` to `1.0`), while PCM integer formats store the *actual*
sample value across the integer's full range. Converting between the two
always requires multiplying by the integer type's max value — a bare cast
from float to int just chops off the fractional part and rounds toward zero,
which silently destroys any float value smaller than `1.0` in magnitude.

## How to verify

Log or print a few values from inside `audioCallback()` (or write them to a
small buffer) after the fix and confirm they are non-zero, roughly
`±16383` for `AMPLITUDE = 0.5f`. Running the emulator with a ROM/timer state
that calls `playSound()` should now produce an audible square-wave tone
instead of silence.
