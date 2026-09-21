# Fix 06: `9XY0` and `BNNN` opcodes were completely missing

**Severity:** High
**File:** `src/chip8.c`
**Function:** `decodeAndExecute()`, outer `switch (opcode & 0xF000)`

## What was wrong

The outer opcode dispatch switch had cases for `0x0`, `0x1`, `0x2`, `0x3`,
`0x4`, `0x5`, `0x6`, `0x7`, `0x8`, `0xA`, `0xC`, `0xD`, `0xE`, and `0xF` — but
no `case 0x9000:` and no `case 0xB000:`. Both instructions are part of the
original, base CHIP-8 instruction set (not an extension), so any ROM that
uses either one fell through to the `default` branch:

```c
default:
    printf("Unknown opcode: 0x%X\n", opcode);
    exit(1);
```

That `exit(1)` terminates the entire emulator process, not just the current
instruction.

## Why it mattered

- `9XY0` ("skip next instruction if VX != VY") is the natural counterpart to
  the already-implemented `5XY0` ("skip if VX == VY") and is used constantly
  for comparisons/loops in real programs.
- `BNNN` ("jump to NNN + V0") is used for jump tables (e.g. menu selection by
  index) and is also common in real ROMs.
- Hitting either opcode didn't produce a wrong result — it killed the whole
  emulator outright, with only a console message and no way to recover.

## The fix

Two new cases were added to the switch, in numeric order alongside the
existing opcodes:

```c
case 0x9000: // 9XY0 : Skip next instruction if VX != VY
    if (chip8->V[(opcode & 0x0F00) >> 8] != chip8->V[(opcode & 0x00F0) >> 4]) {
        chip8->PC += 4; // Skip the next instruction
    } else {
        chip8->PC += 2; // Move to next instruction
    }
    break;
```

This mirrors `5XY0`'s structure exactly, just with `!=` instead of `==`.

```c
case 0xB000: // BNNN : Jump to address NNN + V0
    chip8->PC = (opcode & 0x0FFF) + chip8->V[0]; // No PC += 2, PC is set directly
    break;
```

Like the other jump opcode (`1NNN`), `BNNN` sets `PC` directly to a computed
address and does **not** add 2 afterward — the jump target itself is the next
instruction to execute, so incrementing further would skip a byte.

## Lesson for next time

When a switch dispatches on an opcode's top nibble, it's worth listing out
all 16 possible values (`0x0`–`0xF`) up front and confirming each one has
either a `case` or an explicit, deliberate decision to leave it unimplemented
— rather than discovering gaps only when a specific ROM happens to hit them
at runtime and the whole program exits.

## How to verify

- Set `V[1] = 5`, `V[2] = 7`, run opcode `0x9120` (`9XY0` with X=1, Y=2).
  Confirm `PC` advances by 4 (values differ, so it skips). Then set
  `V[2] = 5` and confirm `PC` advances by only 2 (values equal, no skip).
- Set `V[0] = 0x10`, run opcode `0xB200` (`BNNN` with NNN=0x200). Confirm
  `PC == 0x210` afterward.
