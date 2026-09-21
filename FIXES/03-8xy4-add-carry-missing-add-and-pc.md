# Fix 03: `8XY4` never performed the addition and never advanced PC

**Severity:** Critical
**File:** `src/chip8.c`
**Opcode:** `8XY4` — "Add VY to VX, set VF on carry"

## What was wrong

```c
case 0x0004: // 8XY4: Add VY to VX, set VF on carry
    if (chip8->V[(opcode & 0x00F0) >> 4] > (0xFF - chip8->V[(opcode & 0xF00) >> 8])) {
        chip8->V[0xF] = 1;
    } else {
        chip8->V[0xF] = 0;
    }
    break;
```

This code only computed whether the addition *would* overflow (the carry
flag), and stored that in `V[0xF]`. It never actually did `VX += VY`, and —
worse — it never incremented `chip8->PC`.

## Why it mattered

- **No PC increment is the critical part.** `decodeAndExecute` is called
  again from `executeCycle` every "cycle" using the current `PC`. If `PC`
  never advances, the *exact same instruction* is fetched and executed
  forever. The whole emulator hangs the instant a ROM executes an `8XY4`.
  This opcode (add with carry) is extremely common — used for anything doing
  16-bit-style pointer math, score counters, or position updates — so almost
  any real game triggers this hang quickly.
- Separately, even if `PC` had advanced, the addition itself was missing, so
  `VX` would never change value, silently breaking any game logic that
  depends on the sum.

## The fix

```c
case 0x0004: { // 8XY4: Add VY to VX, set VF on carry
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t carry = (chip8->V[y] > (0xFF - chip8->V[x])) ? 1 : 0;
    chip8->V[x] += chip8->V[y]; // VX += VY
    chip8->V[0xF] = carry; // Carry flag set after the add, so it wins if X == 0xF
    chip8->PC += 2; // Move to next instruction
    break;
}
```

Two things were added:

1. `chip8->V[x] += chip8->V[y];` — the actual addition that was missing.
2. `chip8->PC += 2;` — advances the program counter, matching every other
   opcode handler in this file.

The carry flag is computed from the *pre-add* values (same overflow check as
before) and stored into a local `carry` variable, then written to `V[0xF]`
**after** the addition. This ordering matters: if `X` happens to be `0xF`
(i.e. the instruction is `8F Y4`, adding into `VF` itself), the final value
of `VF` must be the carry flag, not the raw sum — writing the carry flag last
guarantees that regardless of which register `X` is.

## Lesson for next time

When an opcode handler's job is "compute something AND change state (advance
PC)", make sure both halves are actually present before adding a `break;`.
Missing `PC += 2` is one of the easiest and most severe classes of bug in a
fetch-decode-execute loop like this one, because the effect isn't a wrong
answer — it's the whole program freezing with no error message.

## How to verify

Set `V[0] = 10`, `V[1] = 5`, run `8014` (`V[0] += V[1]`), and confirm:
- `V[0] == 15`
- `PC` advanced by 2
- `V[0xF] == 0` (no overflow)

Then set `V[0] = 250`, `V[1] = 10`, run `8014` again and confirm `V[0]` wraps
to `4` (250+10=260, truncated to 8 bits) and `V[0xF] == 1` (overflow
detected).
