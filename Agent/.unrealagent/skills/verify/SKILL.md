---
name: verify
description: Self-verify that the change you just made actually behaves as intended in the live editor/runtime, using a viewport capture and the full output log, then report.
---

Carry this out **continuously to completion**. After verifying, report the result clearly.

## Purpose
Directly observe whether the most recent change (code/widget/actor) actually works. Do not
stop at "it looks correct in the code" — gather real on-screen and log evidence.

## Procedure
1. **Visual check**: Use `capture_viewport` to capture the current editor/play screen and
   confirm the change appears as intended (widget placement, actor state, crosshair, etc.).
2. **Log check**: Use `get_output_log` to inspect the relevant logs. **Check Warnings and
   Errors, not just Info** — a warning/error signals the change works only partially or has
   side effects.
3. **Console check (optional)**: If needed, use `run_console_command` to print state (stat,
   relevant show flags, etc.) for additional evidence.
4. **Verdict**: If it works as intended, report "OK" with the evidence of what you confirmed.
   Otherwise, report the observed mismatch/warning concretely and propose likely causes.

## Report format
- What was confirmed: (facts observed on screen/in logs)
- Result: OK / Has issues
- If issues: observed symptom + suggested next action
