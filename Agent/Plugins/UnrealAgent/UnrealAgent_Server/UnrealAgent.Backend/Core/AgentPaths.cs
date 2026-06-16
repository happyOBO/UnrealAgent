namespace UnrealAgent.Backend.Core;

/// <summary>
/// 프로젝트 경로를 제공하는 정적 클래스입니다.
/// </summary>
public static class AgentPaths
{
    /// <summary>~User/.unrealagent 사용자 설정 디렉터리 경로입니다.</summary>
    public static readonly string UserConfigDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".unrealagent");
    
    // ── 프로젝트 경로 ──

    /// <summary>UE 프로젝트 루트 경로입니다 (.uproject 파일이 위치한 디렉토리).</summary>
    public static string RootPath { get; } = string.Empty;

    /// <summary>.uproject 파일 경로입니다.</summary>
    public static string UProjectPath { get; } = string.Empty;

    /// <summary>프로젝트 레벨 설정 디렉토리 경로입니다 ({RootPath}/.unrealagent).</summary>
    public static string ConfigDir => Path.Combine(RootPath, ".unrealagent");
    
    /// <summary>스킬 디렉토리 경로입니다 ({ConfigDir}/skills).</summary>
    public static string SkillsDir => Path.Combine(ConfigDir, "skills");

    /// <summary>저장된 세션 파일 경로입니다 ({ConfigDir}/session.json). Restore Context의 대상입니다.</summary>
    public static string SessionFilePath => Path.Combine(ConfigDir, "session.json");

    /// <summary>
    /// UE 도메인 컨텍스트 문서 디렉토리 경로입니다.
    /// 플러그인과 함께 배포되어 실행 파일 옆 contexts/에 복사됩니다.
    /// </summary>
    public static string ContextsDir => Path.Combine(AppContext.BaseDirectory, "contexts");
    
    // ── 팀 경로 ──
    
    /// <summary>전체 팀 루트 디렉토리입니다 ({ConfigDir}/teams).</summary>
    public static string TeamsRoot => Path.Combine(ConfigDir, "teams");

    /// <summary>특정 팀의 디렉토리 경로를 반환합니다 ({ConfigDir}/teams/{TeamName}).</summary>
    public static string GetTeamDir(string TeamName) => Path.Combine(TeamsRoot, TeamName);

    /// <summary>팀 메일박스 디렉토리 경로를 반환합니다 ({ConfigDir}/teams/{TeamName}/mailbox).</summary>
    public static string GetMailboxDir(string TeamName) => Path.Combine(GetTeamDir(TeamName), "mailbox");
    
    // ── 초기화 ──

    static AgentPaths()
    {
        DirectoryInfo? Dir = new(AppContext.BaseDirectory);
        while (Dir is not null)
        {
            FileInfo? UProject = Dir.GetFiles("*.uproject").FirstOrDefault();
            
            if (UProject is not null)
            {
                RootPath = Dir.FullName;
                UProjectPath = UProject.FullName;
                
                return;
            }
            
            Dir = Dir.Parent;
        }
    }
}