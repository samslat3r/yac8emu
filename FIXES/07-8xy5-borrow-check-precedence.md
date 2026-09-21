# Fix 07: `8XY5` borrow check used the wrong register due to operator precedence

**Severity:** High
**File:** `src/chip8.c`
**Opcode:** `8XY5` — "Subtract VY from VX, set VF if no borrow"

## What was wrong

```c
if (chip8->V[(opcode & 0x00F0) >> 4] > chip8->V[(opcode & 0x0F00 >> 8)]) {
```

Look closely at the right-hand side: `(opcode & 0x0F00 >> 8)`. In C, the
shift operator `>>` binds **tighter** than the bitwise-AND operator `&`. That
means this expression is *not* `(opcode & 0x0F00) >> 8` (which is the correct
way to extract register `X`) — it actually parses as:

```c
opcode & (0x0F00 >> 8)
```

`0x0F00 >> 8` evaluates to `0x0F` first, so the whole expression becomes
`opcode & 0x0F` — the **low nibble of the opcode**, which is a completely
different, meaningless value here (for `8XY5` that nibble is always `5`,
since it's what identifies this as the "subtract" sub-opcode in the first
place). So the borrow check was comparing `V[Y]` against `chip8->V[5]`
(or whatever `opcode & 0x0F` happened to be), not against `V[X]` — while
the actual subtraction two lines below it did correctly use
`chip8->V[(opcode & 0x0F00) >> 8]`.

## Why it mattered

The borrow flag (`VF`) is how CHIP-8 programs detect whether a subtraction
underflowed (went negative and wrapped around). Because the comparison used
the wrong register, `VF` ended up set to whatever the comparison against the
*wrong* value happened to produce — effectively random from the game's
point of view. Any game logic branching on "did that subtraction borrow?"
(health/damage calculations, boundary checks, etc.) would behave
unpredictably.

## The fix

```c
if (chip8->V[(opcode & 0x00F0) >> 4] > chip8->V[(opcode & 0x0F00) >> 8]) {
    chip8->V[0xF] = 0; // Borrow occured
} else {
    chip8->V[0xF] = 1; // No borrow
}
chip8->V[(opcode & 0x0F00) >> 8] -= chip8->V[(opcode & 0x00F0) >> 4]; //VX -= VY
```

Just adding the missing parentheses — `(opcode & 0x0F00) >> 8` instead of
`(opcode & 0x0F00 >> 8)` — makes the borrow check use the same `X` register
that the subtraction below it uses, which is what the original comment
("some turmoil occured here") suggests someone already suspected but didn't
track down. That stray comment was removed since the actual problem is now
fixed and documented here instead.

## Lesson for next time

`&` and `>>` do not have the precedence you might intuitively expect —
`>>` (and `<<`) bind tighter than `&`, `|`, and `^`. **Always parenthesize
bit-mask-then-shift expressions explicitly**: `(opcode & 0x0F00) >> 8`, never
`opcode & 0x0F00 >> 8`. This exact pattern (`X & MASK >> N`) is one of the
most common real-world C bugs; when in doubt, add the parentheses even if
they turn out to be "redundant" for a particular compiler's actual behavior
here — they make the intent unambiguous to the next reader.

## How to verify

Set `V[0] = 3` (VX), `V[1] = 5` (VY), run `8015` (`8XY5` with X=0, Y=1,
i.e. `VX -= VY`). Since `VY (5) > VX (3)`, this should borrow: confirm
`V[0xF] == 0` and `V[0]` wraps to `254` (3 - 5 = -2, wrapped as `uint8_t`).
Then try the reverse (`V[0] = 5`, `V[1] = 3`): confirm `V[0xF] == 1` (no
borrow) and `V[0] == 2`.
