---
name: plan-doc
description: Investigate the current Unreal project and write a detailed design/planning document into Docs/ — goal, current state, reusable items, Part/Phase plan, risks, scope — formatted so /task-doc can implement it directly.
---

Carry this out **continuously to completion**. Do not stop after investigating — finish by
writing the complete design document. This skill ONLY produces the planning document; it must
**not** implement code, modify assets, or build. Implementation is `/task-doc`'s job.

## Input
The feature/request to plan (e.g. "inventory drag-and-drop", "weapon recoil system"). If none
is specified, ask first what to plan.

## Principle — evidence first, no guessing
Base every statement on what you actually found in the project. NEVER invent class names,
asset paths, GameplayTags, or property values. Query them. When a detail is uncertain after
investigation, **make an explicit assumption and record it in the Open Questions section**
rather than blocking — but if a genuinely blocking ambiguity prevents a sound plan, ask the
user before writing.

## Step 1 — Investigate the project (always first)
Gather concrete evidence before writing anything:
1. **Source & conventions**: Use `Grep`/`Glob`/`Read` over the project's C++/Blueprint source
   for existing classes, components, structs, and enums relevant to the request. Capture the
   real naming prefixes and module layout (e.g. `U*Component`, `A*Character`, `WBP_`,
   `EItemGrade`, `GA_/GE_/DA_` asset prefixes). Cite file paths as evidence.
2. **GameplayTags**: Discover the existing tag scheme — search `Config/DefaultGameplayTags.ini`
   and `UE_DEFINE_GAMEPLAY_TAG*` with `Grep`, or query live tags via `execute_python`. Any new
   tag in the plan must EXTEND the existing hierarchy, not redefine an existing concept under a
   new name.
3. **Live editor / asset state**: Use the MCP query tools (search assets, list level actors,
   inspect blueprints/widgets with `list_widgets`/`list_nodes`, `get_output_log`) to find
   systems and DataAssets that are already built. Never guess asset paths — query them.
4. **Existing `Docs/`**: Read the current docs to learn the folder's file-naming pattern,
   section structure, and **prose language** (Korean `_KR` vs English).

## Step 2 — Write the design document into `Docs/`
Write ONE markdown file under `Docs/`, named to match the existing convention (UPPER_SNAKE;
`PLAN_<feature>` or `<FEATURE>_DESIGN`, with a `_KR` suffix when the folder is Korean). Include
these sections, in this order — they map 1:1 onto `/task-doc`'s Step 1 consistency check:
- **Goal** — what the feature must achieve.
- **Current State** — what already exists, with evidence (file paths / asset paths / tags).
- **Reuse & Duplication** — existing classes, components, tags, and DataAssets to **reuse
  instead of recreating** (feeds task-doc's duplicate-asset check).
- **GameplayTag Plan** — new/extended tags, aligned to the existing hierarchy.
- **Naming & Code Conventions** — the prefixes and patterns the implementation must follow.
- **Scope** — In scope / Out of scope / already-implemented exclusions (one-line reason each).
- **Implementation Plan (Part/Phase)** — ordered phases. Each phase must leave a **compilable
  state**. Mark each step as C++ / Blueprint-widget / editor-only manual work.
- **Risks** — technical risks, order-dependent steps, destructive or irreversible operations.
- **Open Questions** — assumptions made during planning, flagged for confirmation.
- **Handoff** — final line, verbatim: `Implement with /task-doc Docs/<this-file>`.

## Project conventions
- **Prose language**: match the existing `Docs/` folder. Default to Korean with a `_KR`
  filename suffix when the folder is empty (the project is Korean).
- **Code identifiers stay verbatim** (English/code) regardless of prose language — class names,
  GameplayTags, function signatures, asset paths. This is what keeps the handoff to `task-doc`
  unambiguous, not the prose language.
- Do not duplicate an existing doc — if a plan for this feature already exists under `Docs/`,
  update it instead of creating a new one.

## Caution
- Do NOT implement, modify assets, or build — write only the document.
- Do NOT invent assets/tags/paths; every concrete reference must come from Step 1.
- Align all proposed tags and names with the existing scheme.
- A plan that proposes recreating something that already exists is a planning failure — the
  Reuse & Duplication section must catch it first.
