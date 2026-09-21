# Fix 02: `8XY1` / `8XY2` / `8XY3` used the wrong bitmask for register Y

**Severity:** Critical
**File:** `src/chip8.c`
**Opcodes:** `8XY1` (OR), `8XY2` (AND), `8XY3` (XOR)

## What was wrong

```c
chip8->V[(opcode & 0x0F00) >> 8] |= chip8->V[(opcode & 0x0F00) >> 4]; // 8XY1
chip8->V[(opcode & 0x0F00) >> 8] &= chip8->V[(opcode & 0x0F00) >> 4]; // 8XY2
chip8->V[(opcode & 0x0F00) >> 8] ^= chip8->V[(opcode & 0x0F00) >> 4]; // 8XY3
```

An opcode like `8XY1` packs two register numbers into its middle two nibbles:
`X` is bits 8–11 (`opcode & 0x0F00`) and `Y` is bits 4–7 (`opcode & 0x00F0`).
To read out `X` you shift the `0x0F00` mask right by 8. To read out `Y` you
have to first mask with `0x00F0` (bits 4–7), **not** `0x0F00` (bits 8–11), and
then shift right by 4.

All three of these opcodes masked with `0x0F00` for **both** operands instead
of switching to `0x00F0` for the second one. So instead of computing
`(opcode & 0x00F0) >> 4` for `Y`, they computed `(opcode & 0x0F00) >> 4`,
which is `X`'s bits shifted one nibble too far — a value that can be as large
as `0xF0` (240).

## Why it mattered

- `chip8->V[(opcode & 0x0F00) >> 4]` can reach `240`, but `V[]` only has 16
  entries. This is another out-of-bounds read, same class of memory
  corruption as Fix 01.
- Even ignoring the crash risk, the actual OR/AND/XOR was computed against
  the wrong register, so `VX |= VY` (etc.) never really combined `VX` with
  `VY` — it combined `VX` with whatever nonsense byte happened to sit past
  the register array.

## The fix

Mask with `0x00F0` for the `Y` operand in all three cases:

```c
chip8->V[(opcode & 0x0F00) >> 8] |= chip8->V[(opcode & 0x00F0) >> 4]; // VX |= VY
chip8->V[(opcode & 0x0F00) >> 8] &= chip8->V[(opcode & 0x00F0) >> 4]; // VX &= VY
chip8->V[(opcode & 0x0F00) >> 8] ^= chip8->V[(opcode & 0x00F0) >> 4]; // VX ^= VY
```

Notice the pattern already used correctly elsewhere in the same file, e.g.
`8XY0` (`chip8->V[(opcode & 0x0F00) >> 8] = chip8->V[(opcode & 0x00F0) >> 4];`)
— that one line was the correct template to copy for `8XY1`/`8XY2`/`8XY3`.

## Lesson for next time

`0x0F00` and `0x00F0` look almost identical but select completely different
nibbles of the opcode. When copy-pasting a line that extracts `X` to build the
line that extracts `Y`, double-check the mask changed from `0x0F00` to
`0x00F0` (and the shift from `>> 8` to `>> 4`) — it's an easy one-character
typo (`0F00` vs `00F0`) with a large effect.

## How to verify

For `8XY1`/`8XY2`/`8XY3`, set two known values in different registers (e.g.
`V[1] = 0x0F`, `V[2] = 0xF0`), run the opcode, and confirm the result matches
plain OR/AND/XOR of those two specific values — not a value influenced by any
other register.
