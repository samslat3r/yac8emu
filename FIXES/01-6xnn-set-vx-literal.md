# Fix 01: `6XNN` was reading a register instead of the literal value

**Severity:** Critical
**File:** `src/chip8.c`
**Opcode:** `6XNN` — "Set VX to NN"

## What was wrong

```c
case 0x6000: // 6XNN : Set VX to NN
    chip8->V[(opcode & 0x0F00) >> 8] = chip8->V[(opcode & 0x00FF)]; // BUG
```

The CHIP-8 instruction `6XNN` means: take the literal 8-bit value `NN` that is
baked into the opcode, and store it directly into register `VX`. For example,
opcode `0x6A05` means "set V[A] = 0x05".

The old code did not do that. It took the `NN` byte and used it as an **index
into the register array itself** (`chip8->V[NN]`), instead of using it as the
value to store. So `0x6A05` was actually doing `V[A] = V[0x05]` — copying
whatever garbage happened to be in register 5, instead of storing the literal
`0x05`.

## Why it mattered

- `V[]` only has 16 elements (`CHIP8_REGISTER_COUNT`), but `NN` ranges from
  `0x00` to `0xFF` (0–255). Any opcode like `6A10` (`NN = 0x10 = 16`) or higher
  read `chip8->V[16]` through `chip8->V[255]` — memory **past the end of the
  array**. In C this is undefined behavior: it can silently read garbage,
  corrupt an adjacent field in `chip8_t` (like `I`, `PC`, or `memory[]`), or
  crash.
- `6XNN` is used constantly by every CHIP-8 program to load constants (loop
  counters, coordinates, colors, etc.). This single bug was enough to make
  almost every ROM misbehave or crash immediately.

## The fix

Use the literal byte directly instead of treating it as an index:

```c
case 0x6000: // 6XNN : Set VX to NN
    chip8->V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF; // Set register VX to NN
    chip8->PC += 2; // Move to next instruction
    break;
```

`opcode & 0x00FF` masks out everything except the low byte (`NN`) and that
value is assigned straight into `VX`. No array indexing on the right-hand
side at all.

## Lesson for next time

When an opcode's operand is described as an *immediate value* (a literal
number baked into the instruction, often written `NN` or `NNN` in CHIP-8
documentation), it should be used as a **plain number**, not as an index into
some other array. Register indices are always the `X`/`Y` nibbles
(`(opcode & 0x0F00) >> 8` and `(opcode & 0x00F0) >> 4`); anything called
`NN`/`NNN`/`KK` is data, not an address of a register.

## How to verify

Any ROM that loads constants into registers (essentially all of them) will
now set the correct values. A minimal manual check: step through an opcode
like `0x6005` and confirm `V[0] == 0x05` afterward, regardless of what was
previously in `V[5]`.
