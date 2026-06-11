namespace UnrealAgent.Backend.Skill;

/// <summary>
/// SKILL.md에서 파싱된 스킬 정의입니다.
/// 프론트매터 메타데이터와 마크다운 본문을 포함합니다.
/// </summary>
public sealed class SkillDefinition
{
    // ── 프론트매터 (Level 1: 시작 시 로딩) ──
    
    /// <summary>스킬 이름입니다. 슬래시 커맨드 및 Skill 도구에서 사용됩니다.</summary>
    public required string Name { get; init; }
    
    /// <summary>스킬 설명입니다. 시스템 프롬프트에 포함되어 모델의 자동 호출 판단 기준이 됩니다.</summary>
    public required string Description { get; init; }
    
    /// <summary>true이면 모델의 자동 호출을 차단하고 사용자 수동 호출만 허용합니다.</summary>
    public bool bDisableModelInvocation { get; init; }
    
    /// <summary>false이면 사용자에게 숨기고 모델만 호출할 수 있습니다.</summary>
    public bool bUserInvocable { get; init; } = true;
    
    /// <summary>SKILL.md 파일이 위치한 디렉토리의 절대 경로입니다.</summary>
    public required string SkillRoot { get; init; }
    
    // ── 본문 (Level 2: 호출 시 로딩) ──
    
    /// <summary>스킬 프롬프트 본문(마크다운)입니다.</summary>
    public required string Content { get; init; }
    
    // ── 메서드 ──

    /// <summary>스킬 본문을 system-reminder 형태로 반환합니다.</summary>
    public string BuildInstruction()
    {
        return $"""
                <system-reminder>
                Skill '{Name}' has been invoked. Follow these instructions:

                {Content}
                </system-reminder>
                """;
    }
}