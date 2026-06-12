using UnrealAgent.Backend.Mode;

namespace UnrealAgent.Backend.ClaudeCode;

/// <summary>
/// claude CLI 1회 실행에 필요한 옵션입니다.
/// AgentLoop가 매 턴 현재 설정(시스템 프롬프트/모델/모드/effort 등)으로 구성합니다.
/// </summary>
public sealed class ClaudeRunOptions
{
    /// <summary>전체 시스템 프롬프트입니다. --system-prompt로 전달됩니다.</summary>
    public required string SystemPrompt { get; init; }

    /// <summary>모델 ID입니다. --model로 전달됩니다 (예: "claude-opus-4-6").</summary>
    public required string Model { get; init; }

    /// <summary>effort 레벨입니다 (low/medium/high/xhigh/max). null이면 생략합니다.</summary>
    public string? Effort { get; init; }

    /// <summary>확장 사고 활성화 여부입니다. false면 MAX_THINKING_TOKENS=0으로 비활성화합니다.</summary>
    public bool ThinkingEnabled { get; init; }

    /// <summary>에이전트 모드입니다. --permission-mode로 매핑됩니다.</summary>
    public required AgentMode Mode { get; init; }

    /// <summary>CLI용 MCP 설정 파일 경로입니다. null이면 생략합니다.</summary>
    public string? McpConfigPath { get; init; }

    /// <summary>이어갈 세션 ID입니다. null이면 새 세션을 시작합니다.</summary>
    public string? ResumeSessionId { get; init; }

    /// <summary>작업 디렉터리입니다 (UE 프로젝트 루트).</summary>
    public string? WorkingDirectory { get; init; }

    /// <summary>API 키입니다. null이면 claude 로그인 자격증명을 사용합니다.</summary>
    public string? ApiKey { get; init; }
}
