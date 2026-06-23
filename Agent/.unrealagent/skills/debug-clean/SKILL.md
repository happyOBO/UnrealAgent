---
name: debug-clean
description: Find and remove temporary code added for debugging (logs, CVars, comments, debug widgets) and restore the original state.
---

Carry this out **continuously to completion**. Do not stop at merely finding candidates —
finish the removal and confirm a compilable state.

## Goal
Remove only the instrumentation code that was added temporarily to track down a problem.
**Do not touch functional code.**

## Removal targets
- Temporary `UE_LOG(...)` / `UE_VLOG` / on-screen `AddOnScreenDebugMessage`.
- Debugging `TAutoConsoleVariable` / CVar (e.g. `CVarDamageMode`) and their branch code.
- Temporary comments (`// TEMP`, `// DEBUG`, `// 디버그용`, etc.) and commented-out
  experimental code.
- Debug-display widgets/text (e.g. AimPitch/Yaw debug HUD) and their update logic.
- Temporary console command registrations, test-only key bindings.

## Procedure
1. **Identify**: Search for the above items added during recent debugging sessions. If it is
   unclear how far "temporary" extends, confirm the candidate list with the user before
   removing (functional logs may be worth keeping).
2. **Remove**: Remove only the instrumentation, and clean up any empty branches, unused
   variables, or broken includes it leaves behind.
3. **Verify**: Confirm a compilable state. If you removed a widget, confirm normal display with
   `capture_viewport`. Use `get_output_log` to confirm no leftover logs remain.
4. **Report**: Summarize the removed items, one line per file.

## Caution
- Do not arbitrarily remove permanently useful operational logs (e.g. an equip-grant success
  log) — confirm with the user. When in doubt, ask.
