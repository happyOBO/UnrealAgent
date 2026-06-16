namespace UnrealAgent.Backend.Token;

/// <summary>
/// 카테고리별 컨텍스트 토큰 사용량입니다.
/// Count Tokens API와 런타임 Usage를 조합하여 산출합니다.
/// </summary>
public sealed record TokenUsage(long SystemPrompt, long UnrealAgentMd, long Skills, long Tools, long Messages, long ContextWindow)
{
    /// <summary>총 입력 토큰 수입니다 (System + UnrealAgentMd + Skills + Tools + Messages).</summary>
    private long TotalInput => SystemPrompt + UnrealAgentMd + Skills + Tools + Messages;
    
    /// <summary>남은 토큰 수입니다. 입력이 윈도우를 초과해도 음수로 표시되지 않도록 0에서 멈춥니다.</summary>
    public long FreeSpace => Math.Max(0, ContextWindow - TotalInput);
    
    /// <summary>컨텍스트 윈도우 사용률(0~100)입니다.</summary>
    public double UsagePercent => ContextWindow > 0 ? (double)TotalInput / ContextWindow * 100 : 0;
}