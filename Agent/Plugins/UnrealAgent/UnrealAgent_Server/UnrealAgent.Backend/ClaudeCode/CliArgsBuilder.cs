using UnrealAgent.Backend.Mode;

namespace UnrealAgent.Backend.ClaudeCode;

/// <summary>
/// ClaudeRunOptions에서 claude CLI 인자 목록을 조립합니다.
/// ProcessStartInfo.ArgumentList에 그대로 넣어 인용 처리를 .NET에 위임합니다.
/// </summary>
public static class CliArgsBuilder
{
    /// <summary>
    /// 지속 스트리밍 모드(-p 없이) 인자를 생성합니다.
    /// control 프로토콜(can_use_tool) 활성화를 위해 --permission-prompt-tool stdio를 포함합니다.
    /// 시스템 프롬프트는 파일로 전달하여 명령행 길이/이스케이프 문제를 회피합니다.
    /// </summary>
    public static List<string> Build(ClaudeRunOptions Options, string SystemPromptFilePath)
    {
        List<string> Args =
        [
            // 구조화된 양방향 스트리밍
            "--input-format", "stream-json",
            "--output-format", "stream-json",
            "--verbose",
            // 권한 control 프로토콜 활성화 (이게 없으면 도구가 자동 거부됨)
            "--permission-prompt-tool", "stdio",
            // 권한 모드 (Normal/Edit/Plan 매핑)
            "--permission-mode", MapPermissionMode(Options.Mode),
            // 모델 선택
            "--model", Options.Model,
            // Claude Code 기본 시스템 프롬프트를 유지하고 UnrealAgent 도메인 지침만 덧붙입니다.
            // (전체 교체 --system-prompt-file → append로 전환: 기본 에이전트 역량 보존)
            "--append-system-prompt-file", SystemPromptFilePath,
            // --mcp-config로 지정한 서버만 사용 (사용자 개인 MCP 서버 차단)
            "--strict-mcp-config",
        ];

        // 사용자 개인 ~/.claude 스킬/슬래시 커맨드가 세션에 로드되는 것을 차단합니다.
        // 스킬은 C#(SkillRegistry)에서 전개되므로 CLI 측 스킬은 불필요합니다.
        // 단, /compact 위임 턴은 CLI 내장 커맨드가 필요하므로 격리를 해제합니다.
        if (!Options.AllowCliSlashCommands)
            Args.Add("--disable-slash-commands");

        // effort 레벨 (Haiku 등 미지원 모델은 호출부에서 null 전달)
        if (!string.IsNullOrEmpty(Options.Effort))
        {
            Args.Add("--effort");
            Args.Add(Options.Effort);
        }

        // 에디터 도구 MCP 설정
        if (!string.IsNullOrEmpty(Options.McpConfigPath))
        {
            Args.Add("--mcp-config");
            Args.Add(Options.McpConfigPath);
        }

        // 세션 이어가기
        if (!string.IsNullOrEmpty(Options.ResumeSessionId))
        {
            Args.Add("--resume");
            Args.Add(Options.ResumeSessionId);
        }

        return Args;
    }

    /// <summary>AgentMode를 CLI --permission-mode 값으로 매핑합니다.</summary>
    public static string MapPermissionMode(AgentMode Mode) => Mode switch
    {
        // 모든 도구 자동 승인 → 권한 검사 우회
        AgentMode.Edit => "bypassPermissions",
        // 계획 전용 (도구 실행 차단)
        AgentMode.Plan => "plan",
        // 기본: 권한 필요한 도구는 can_use_tool로 사용자에게 확인
        _ => "default",
    };
}
