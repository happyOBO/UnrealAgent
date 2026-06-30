<#
.SYNOPSIS
  PreToolUse hook: auto-checkout read-only source files from Perforce before Edit/Write/MultiEdit.

.DESCRIPTION
  In a Perforce workspace every tracked file (.cpp/.h/.cs/...) is read-only until checked out,
  so Claude Code's Edit/Write would fail. This hook runs before those tools and:
    1. Reads the hook payload (JSON) from stdin and pulls tool_input.file_path.
    2. Allows immediately if the path is missing, the file does not exist (new file), or it is
       already writable.
    3. Otherwise runs `p4 edit <file>` to check the file out.
    4. If the file is now writable, allows the edit.
    5. If checkout still failed (p4 not installed / not under a workspace / file exclusively
       locked), BLOCKS the edit and tells the agent to confirm with the user before forcing the
       read-only flag off. This mirrors the asset-side CHECKOUT_REQUIRED confirmation flow.

  To force after the user agrees, clear the read-only flag, e.g.:
    PowerShell:  Set-ItemProperty -LiteralPath '<file>' -Name IsReadOnly -Value $false
    cmd:         attrib -r "<file>"

  Exit codes: 0 = allow; 2 = block (stderr is shown to the agent).
#>

$ErrorActionPreference = 'Stop'

function Allow { exit 0 }

try {
    $raw = [Console]::In.ReadToEnd()
    if ([string]::IsNullOrWhiteSpace($raw)) { Allow }

    $payload = $raw | ConvertFrom-Json
    $filePath = $payload.tool_input.file_path
    if ([string]::IsNullOrWhiteSpace($filePath)) { Allow }

    # New file (being created) -> nothing to check out.
    if (-not (Test-Path -LiteralPath $filePath)) { Allow }

    $item = Get-Item -LiteralPath $filePath
    if (-not $item.IsReadOnly) { Allow }  # already writable

    # Read-only: try a Perforce checkout. Run from the file's directory so p4 picks up the
    # workspace/P4CONFIG context.
    $dir = [System.IO.Path]::GetDirectoryName($filePath)
    $p4 = Get-Command p4 -ErrorAction SilentlyContinue
    if ($p4) {
        Push-Location -LiteralPath $dir
        try {
            & p4 edit -- "$filePath" *> $null
        } catch {
            # swallow; handled by the writability re-check below
        } finally {
            Pop-Location
        }

        # Re-check: p4 edit clears the read-only flag on success.
        $item.Refresh()
        if (-not (Get-Item -LiteralPath $filePath).IsReadOnly) { Allow }
    }

    # Still read-only -> block and ask for confirmation.
    $msg = @"
CHECKOUT_REQUIRED: '$filePath' is read-only and a Perforce checkout did not make it writable
(p4 missing, file not under a workspace, or exclusively locked by someone else).
Ask the user whether to force-clear the read-only flag and edit anyway. If they agree, run:
  Set-ItemProperty -LiteralPath '$filePath' -Name IsReadOnly -Value `$false
then retry the edit. If they decline, stop and report this.
"@
    [Console]::Error.WriteLine($msg)
    exit 2
}
catch {
    # Never hard-fail the edit because of a hook bug; let it proceed and surface the error.
    [Console]::Error.WriteLine("p4-checkout hook error (allowing edit): $($_.Exception.Message)")
    exit 0
}
