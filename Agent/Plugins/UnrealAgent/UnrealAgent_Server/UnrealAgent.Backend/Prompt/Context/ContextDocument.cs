namespace UnrealAgent.Backend.Prompt.Context;

/// <summary>
/// 도메인별 UE Python API 컨텍스트 문서입니다.
/// 프론트매터의 키워드로 사용자 메시지와 매칭되어 user 턴에 주입됩니다.
/// </summary>
public sealed class ContextDocument
{
    /// <summary>도메인 이름입니다 (예: "actor", "material").</summary>
    public required string Name { get; init; }

    /// <summary>도메인 설명입니다 (디버그/로깅용).</summary>
    public required string Description { get; init; }

    /// <summary>매칭에 사용할 키워드 목록입니다 (모두 소문자).</summary>
    public required IReadOnlyList<string> Keywords { get; init; }

    /// <summary>주입할 마크다운 본문입니다.</summary>
    public required string Content { get; init; }
}
