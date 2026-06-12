namespace UnrealAgent.Backend.Model;

/// <summary>
/// 모델의 확장 사고(Extended Thinking) 지원 방식입니다.
/// </summary>
public enum ThinkingSupport
{
    /// <summary>사용자가 켜고 끌 수 있습니다.</summary>
    Toggle,
    /// <summary>항상 켜져 있으며 끌 수 없습니다 (예: Fable 5).</summary>
    AlwaysOn,
    /// <summary>지원하지 않습니다 (예: Haiku 4.5).</summary>
    Unsupported,
}
