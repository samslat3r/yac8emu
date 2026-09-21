# Fix 08: Call stack had no overflow/underflow protection

**Severity:** High
**File:** `src/chip8.c`
**Opcodes:** `2NNN` (call subroutine), `00EE` (return from subroutine)

## What was wrong

```c
// 2NNN - Call subroutine at NNN (before)
case 0x2000:
    chip8->stack[chip8->SP] = chip8->PC; // Store current PC on stack
    chip8->SP++; // Increment stack pointer
    chip8->PC = opcode & 0x0FFF; // Jump to subroutine at NNN
    break;
```

```c
// 00EE - Return from a subroutine (before)
case 0x00EE:
    chip8->SP--; // Decrement stack pointer
    chip8->PC = chip8->stack[chip8->SP]; // Move to address at top of stack
    chip8->PC += 2; // Move to next instruction
    break;
```

`chip8->stack` is a fixed-size array of 16 entries (`CHIP8_STACK_SIZE`), and
`chip8->SP` is a `uint8_t` used as the index into it.

- **`2NNN` (call):** nothing checked that `SP < CHIP8_STACK_SIZE` before
  writing `stack[SP]` and incrementing. A ROM that calls subroutines more
  than 16 levels deep (a runaway recursive routine, or one triggered by
  another bug like the `8XY4` infinite-loop issue) would write past the end
  of the 16-entry array.
- **`00EE` (return):** nothing checked that `SP > 0` before decrementing. A
  stray or malformed `RET` with an empty call stack (`SP == 0`) would
  decrement `SP` past zero. Since `SP` is `uint8_t` (unsigned), `0 - 1` wraps
  around to `255`, and the code would then read `stack[255]` — 239 entries
  past the end of a 16-element array.

## Why it mattered

Both are genuine out-of-bounds memory accesses (write, in the overflow case;
read, in the underflow case) — undefined behavior in C, and a real risk for
an interpreter whose whole job is to run arbitrary, possibly buggy or
adversarial, ROM files. This isn't a "can't happen" edge case: any malformed
or intentionally malicious ROM can trigger it directly, and a bug elsewhere
in the interpreter (corrupting `PC`, causing spurious calls/returns) could
trigger it indirectly too.

## The fix

```c
// 2NNN - Call subroutine at NNN (after)
case 0x2000:
    if (chip8->SP >= CHIP8_STACK_SIZE) { // No room left on the stack
        printf("Stack overflow: call depth exceeded %d\n", CHIP8_STACK_SIZE);
        exit(1);
    }
    chip8->stack[chip8->SP] = chip8->PC;
    chip8->SP++;
    chip8->PC = opcode & 0x0FFF;
    break;
```

```c
// 00EE - Return from a subroutine (after)
case 0x00EE:
    if (chip8->SP == 0) { // Nothing on the stack to return to
        printf("Stack underflow: RET with empty call stack\n");
        exit(1);
    }
    chip8->SP--;
    chip8->PC = chip8->stack[chip8->SP];
    chip8->PC += 2;
    break;
```

Both checks are added **before** touching `stack[]` or `SP`, so the
out-of-bounds access can never happen. On failure they report a clear,
specific error message and stop the emulator via `exit(1)` — the same
pattern already used everywhere else in `decodeAndExecute()` for the
"Unknown opcode" case, so this is consistent with the rest of the file
rather than introducing a new error-handling style. A future improvement
could recover more gracefully (e.g. clamp and continue, or return to the
caller with a logged warning) instead of exiting, but a hard stop with a
clear message is a safe, minimal fix that removes the memory-safety issue.

## Lesson for next time

Any time an index into a fixed-size array is incremented or decremented
based on external/untrusted input (here, the ROM's own instructions), the
bounds check has to happen **before** the array access, not be assumed. "It
shouldn't happen in a well-formed program" is not a safety guarantee for
code that runs arbitrary files — treat ROM input the same way you'd treat
any other untrusted input.

## How to verify

- **Overflow:** feed the emulator a ROM that calls subroutines 17+ levels
  deep without returning. Before the fix this would corrupt memory near the
  `stack` array; after the fix it should print `Stack overflow: call depth
  exceeded 16` and exit cleanly instead.
- **Underflow:** feed the emulator a ROM whose very first instruction is
  `00EE` (`RET`) with nothing ever called. Before the fix this would read
  `stack[255]`; after the fix it should print `Stack underflow: RET with
  empty call stack` and exit cleanly instead.
