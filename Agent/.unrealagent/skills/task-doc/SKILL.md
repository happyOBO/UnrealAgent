---
name: task-doc
description: Implement work based on a task/design document in the Docs folder. Automatically checks existing conventions, duplication, and already-implemented items before starting.
---

Carry this out **continuously to completion**. Even as context grows long, do not stop after
a single step. Never leave a file half-finished — complete and save the file you are currently
editing before moving on.

## Input
The task document the user specifies (e.g. `Docs/INVENTORY_SYSTEM_KR.md`,
`Docs/TASK_aim_pipeline_phase1.md`, `Docs/DEVELOPMENT_PLAN_KR.md`). If none is specified, ask
first which document to base the work on.

## Step 1 — Pre-start consistency check (always first)
Do not implement the document right away; first confirm its consistency with the current
project:
1. **Duplicate features/assets**: Search whether the classes, GameplayTags, DataAssets, or
   assets the document asks you to create already exist in the project. If they do, reuse the
   existing ones instead of creating new ones.
2. **GameplayTag naming**: Check that the document's tag names do not redundantly define the
   same concept under a different name than the existing tag scheme. Align with the existing
   tag naming.
3. **Code conventions**: Match the existing code style (naming, module structure, include
   patterns).
4. **Already-implemented / out-of-scope items**: Exclude features that the document lists but
   are already implemented (e.g. parts of a system) or are out of current scope (e.g. sprint),
   and report the reason for exclusion in one line.

If the check finds the document conflicting with the current project, do not force the
implementation — first report the conflict points and recommended adjustments to the user.

## Step 2 — Implementation
- Proceed in the document's order (Part/Phase). Keep a compilable state at the end of each step.
- Do in code what can be done in code. For **manual work only possible in the Unreal Editor**
  (asset placement, some Blueprint pin connections, and other parts not doable via tools), do
  not arbitrarily skip it — write it up as a manual guide document under `Docs/`, together with
  the reason no tool can automate it.
- For Blueprint/widget work, try widget_modify and blueprint_modify first. If not possible,
  give manual guidance.

## Step 3 — Verification
- For C++ changes, compile (or ask the user to build) and confirm zero errors.
- If there is runtime impact, check warnings/errors with `get_output_log`.
- When done: report a summary distinguishing what was implemented / what was excluded / what
  was left as manual work.
