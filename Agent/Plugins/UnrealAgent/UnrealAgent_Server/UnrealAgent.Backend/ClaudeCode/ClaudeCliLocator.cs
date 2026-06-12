using System.Diagnostics;

namespace UnrealAgent.Backend.ClaudeCode;

/// <summary>
/// claude CLI 실행 파일 경로를 탐색하고 캐싱합니다.
/// (UnrealClaude FClaudeCodeRunner::GetClaudePath 포팅)
/// </summary>
public sealed class ClaudeCliLocator
{
    /// <summary>탐색 결과 캐시입니다. 한 번 찾으면 재사용합니다.</summary>
    private string? Cached;

    /// <summary>
    /// claude CLI 경로를 반환합니다. 찾지 못하면 null입니다.
    /// </summary>
    public string? Find()
    {
        if (Cached is not null)
            return Cached;

        foreach (string Candidate in EnumerateCandidates())
        {
            if (File.Exists(Candidate))
                return Cached = Candidate;
        }

        // PATH 탐색 (where/which) 폴백
        return Cached = ResolveViaWhere();
    }

    /// <summary>플랫폼별 설치 위치 후보를 나열합니다.</summary>
    private static IEnumerable<string> EnumerateCandidates()
    {
        if (OperatingSystem.IsWindows())
        {
            string? UserProfile = Environment.GetEnvironmentVariable("USERPROFILE");
            if (!string.IsNullOrEmpty(UserProfile))
                yield return Path.Combine(UserProfile, ".local", "bin", "claude.exe");

            string? AppData = Environment.GetEnvironmentVariable("APPDATA");
            if (!string.IsNullOrEmpty(AppData))
                yield return Path.Combine(AppData, "npm", "claude.cmd");

            string? LocalAppData = Environment.GetEnvironmentVariable("LOCALAPPDATA");
            if (!string.IsNullOrEmpty(LocalAppData))
                yield return Path.Combine(LocalAppData, "npm", "claude.cmd");

            foreach (string Dir in SplitPath(';'))
            {
                yield return Path.Combine(Dir, "claude.cmd");
                yield return Path.Combine(Dir, "claude.exe");
            }
        }
        else
        {
            string? Home = Environment.GetEnvironmentVariable("HOME");
            if (!string.IsNullOrEmpty(Home))
                yield return Path.Combine(Home, ".local", "bin", "claude");

            yield return "/usr/local/bin/claude";
            yield return "/usr/bin/claude";

            foreach (string Dir in SplitPath(':'))
                yield return Path.Combine(Dir, "claude");
        }
    }

    /// <summary>PATH 환경변수를 구분자로 분리합니다.</summary>
    private static IEnumerable<string> SplitPath(char Separator)
        => (Environment.GetEnvironmentVariable("PATH") ?? "")
            .Split(Separator, StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

    /// <summary>where(Windows)/which(Unix)로 PATH에서 claude를 찾습니다.</summary>
    private static string? ResolveViaWhere()
    {
        try
        {
            ProcessStartInfo Psi = OperatingSystem.IsWindows()
                ? new ProcessStartInfo("where") { ArgumentList = { "claude" } }
                : new ProcessStartInfo("/bin/sh") { ArgumentList = { "-c", "which claude" } };

            Psi.RedirectStandardOutput = true;
            Psi.UseShellExecute = false;
            Psi.CreateNoWindow = true;

            using Process? P = Process.Start(Psi);
            if (P is null)
                return null;

            string Output = P.StandardOutput.ReadToEnd();
            P.WaitForExit(5000);

            string First = Output
                .Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
                .FirstOrDefault() ?? "";

            return string.IsNullOrEmpty(First) ? null : First;
        }
        catch
        {
            return null;
        }
    }
}
