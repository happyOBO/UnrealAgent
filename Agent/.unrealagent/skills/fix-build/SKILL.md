---
name: fix-build
description: Diagnose and fix pasted C++ compile errors. Resolves missing includes, forward declarations, and overloads, then verifies by rebuilding.
---

Carry this out **continuously to completion**. Do not stop at merely finding the errors —
apply the fix and finish with verification.

## Input
The compiler output the user pasted (MSVC error codes C####). If there is no error text,
ask for it first.

## Procedure
1. **Parse errors**: Extract each error's file, line, error code, and message. Handle the
   first (root) error first — the errors that follow are often cascades of it.
2. **Classify the cause** (types seen often in Shooter):
   - `C1083 'X.h': No such file or directory` → missing include path / module dependency.
     Verify the correct header path (e.g. `Kismet/KismetAnimationLibrary.h`) and check
     PublicDependencyModuleNames in Build.cs.
   - `C2027 use of undefined type 'UX'` / `C2065` → only a forward declaration exists, the
     defining header is not included. Include the type's actual header (e.g. GameplayEffect.h).
   - `C2601 local function definitions are illegal` → function signature/brace mismatch,
     missing `_Implementation`, etc.
   - `C2665 no overloaded function could convert` (e.g. GetNameSafe) → argument type mismatch.
     Cast/fix the arguments to match the correct overload.
3. **Apply the fix**: Fix with the minimal change. Follow the existing code style and include
   order.
4. **Verify**: If a build tool is available, rebuild; otherwise ask the user to build, and be
   ready to handle the next error in sequence.
5. **Report**: One-line summary per error — what the cause was and how you fixed it.

## Caution
- Do not paper over an error with temporary comment-outs or by removing functionality — fix
  the root cause.
- Judge against the UE5.7 API (do not introduce deprecated APIs).
