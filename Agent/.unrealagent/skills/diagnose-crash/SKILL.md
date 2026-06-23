---
name: diagnose-crash
description: Analyze the cause of a runtime crash (access violation, call stack, GAS ASC NULL, etc.) based on log evidence. No guessing.
---

Carry this out **continuously to completion**. Do not stop at merely listing candidate causes
— narrow them with evidence and present the single most likely cause along with a concrete fix.

## Principles
- **No guessing, evidence first**: Narrow the conclusion using only the call stack, logs, and
  repro conditions. If unsure, state which log/debug step to use to confirm.
- Actively use **binary clues** the user gives — e.g. "commenting out one line makes the crash
  go away." Suspect the lifetime/validity of the object that line touches as the top priority.

## Procedure
1. **Identify the crash type**:
   - `Exception 0xc0000005 / Access violation reading location` → dereference of an invalid
     pointer. If the read address is small (0x0000xxxx) it is null-ish; otherwise it is likely
     dangling / use-after-free.
   - If the call stack points **inside the allocator** (`mi_page_malloc` /
     `FMallocMimalloc::Realloc`, etc.), the real cause may be heap corruption in an earlier
     frame (already-corrupted memory that the allocator touches later) → look for the code that
     corrupted the heap, not the top of the call stack.
2. **GAS-specific checks** (frequent in Shooter):
   - Crash near `ApplyGameplayEffectSpecToTarget` → check validity of `TargetASC`/`SourceASC`,
     whether `DamageSpec.Data.Get()` is null, and validity of the GameplayEffect class.
   - A `TargetASC=NULL` pattern in the log → an initialization-timing problem of the target's
     PlayerState/ASC.
3. **Establish repro conditions**: Confirm which action (firing while moving, taking a hit from
   an opponent, etc.) reproduces it 100% / intermittently. If it depends on multiplayer, account
   for the fact that it may be hard to reproduce in standalone.
4. **Memory debugging guidance**: When heap corruption is suspected, advise enabling
   `-stompalloc` (in Rider, add `-stompalloc` to the program arguments of the run/debug
   configuration). After applying, confirm whether the crash point moves to the actual
   corrupting code.
5. **Fix or instrument**: If the cause is pinned down, fix it. If not yet, add validity-check
   logging (object name/pointer) to narrow it on the next repro.

## Report
The single most likely cause + evidence + fix. If uncertain, state "what to check on the next
repro."
