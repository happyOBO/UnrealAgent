using System.Text.Json;
using UnrealAgent.Backend.Core;
using UnrealAgent.Backend.Mode;

namespace UnrealAgent.Backend.Security;

/// <summary>
/// 도구 실행 권한 엔진입니다.
/// </summary>
public sealed class PermissionEngine(Team.Team Team)
{
    /// <summary>항상 허용되는 도구 이름 집합입니다.</summary>
    private readonly HashSet<string> AllowedTools = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>파일을 수정하는 CLI 내장 도구 이름들입니다 (file_path 검사 대상).</summary>
    private static readonly HashSet<string> FileEditTools = new(StringComparer.OrdinalIgnoreCase)
    {
        "Edit", "Write", "MultiEdit", "NotebookEdit"
    };

    /// <summary>도구를 허용 목록에 추가합니다.</summary>
    public void Allow(string ToolName) => AllowedTools.Add(ToolName);

    /// <summary>도구 호출의 실행 권한을 조회합니다.</summary>
    public Task<ToolPermission> GetPermissionAsync(Block.ToolUse ToolCall, AgentMode Mode)
    {
        // 보호 경로 편집 차단: 모드/팀 자동 허용보다 먼저 검사하며, 승인 없이 하드 거부합니다.
        // 대상: 모든 플러그인(./Plugins/**)과 프로젝트 작업 디렉터리 밖 절대경로.
        // 에이전트가 자신/타 플러그인 소스나 프로젝트 외부 파일을 임의 편집하지 못하게 합니다.
        if (TargetsProtectedPath(ToolCall))
            return Task.FromResult(ToolPermission.Deny);

        // 팀원 모드는 모든 도구 허용
        if (Team.ParentPid is not null)
            return Task.FromResult(ToolPermission.Allow);

        // Edit 모드 확인
        if (Mode == AgentMode.Edit)
            return Task.FromResult(ToolPermission.Allow);

        // 허용 목록 확인
        if (AllowedTools.Contains(ToolCall.Name))
            return Task.FromResult(ToolPermission.Allow);

        // 사용자에게 확인을 요청
        return Task.FromResult(ToolPermission.Ask);
    }

    /// <summary>
    /// 파일 편집 도구가 보호 대상 경로를 수정하려는지 판정합니다.
    /// </summary>
    private static bool TargetsProtectedPath(Block.ToolUse ToolCall)
    {
        if (!FileEditTools.Contains(ToolCall.Name))
            return false;

        string? FilePath = TryGetFilePath(ToolCall.InputJson);
        if (string.IsNullOrEmpty(FilePath))
            return false;

        return IsProtectedPath(FilePath);
    }

    /// <summary>InputJson에서 file_path 값을 추출합니다. 실패 시 null.</summary>
    private static string? TryGetFilePath(string InputJson)
    {
        if (string.IsNullOrEmpty(InputJson))
            return null;

        try
        {
            using JsonDocument Doc = JsonDocument.Parse(InputJson);
            return Doc.RootElement.TryGetProperty("file_path", out JsonElement Path) ? Path.GetString() : null;
        }
        catch
        {
            return null;
        }
    }

    /// <summary>경로를 슬래시·소문자로 정규화합니다.</summary>
    private static string Normalize(string Path) => Path.Replace('\\', '/').ToLowerInvariant();

    /// <summary>
    /// 보호 대상 경로인지 판정합니다.
    /// (a) 프로젝트 작업 디렉터리 밖이거나, (b) 프로젝트 내 Plugins/ 하위면 차단합니다.
    /// </summary>
    private static bool IsProtectedPath(string FilePath)
    {
        string Root = AgentPaths.RootPath;

        // 프로젝트 루트를 모르면 보수적으로 세그먼트 매칭(플러그인/서버 소스)으로 폴백합니다.
        if (string.IsNullOrEmpty(Root))
        {
            string Seg = Normalize(FilePath);
            return Seg.Contains("/plugins/") || Seg.Contains("/unrealagent_server/");
        }

        string Full;
        try
        {
            // 상대경로는 프로젝트 작업 디렉터리 기준으로 절대경로화합니다.
            Full = Normalize(System.IO.Path.GetFullPath(FilePath, Root));
        }
        catch
        {
            return false;
        }

        string NormRoot = Normalize(System.IO.Path.GetFullPath(Root)).TrimEnd('/');

        // (a) 프로젝트 밖
        bool InsideProject = Full == NormRoot || Full.StartsWith(NormRoot + "/", StringComparison.Ordinal);
        if (!InsideProject)
            return true;

        // (b) 프로젝트 내 Plugins/ 하위
        return Full.StartsWith(NormRoot + "/plugins/", StringComparison.Ordinal);
    }
}
