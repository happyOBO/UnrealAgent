namespace UnrealAgent.Backend.Command.Attributes;

/// <summary>
/// CommandRegistry가 자동 스캔하는 슬래시 커맨드 마커 어트리뷰트입니다.
/// </summary>
[AttributeUsage(AttributeTargets.Class, Inherited = false)]
public sealed class AgentCommandAttribute(string name, string description, string icon = "terminal") : Attribute
{
    /// <summary>슬래시 커맨드 이름입니다 (예: "/clear").</summary>
    public string Name { get; } = name;

    /// <summary>사용자에게 표시할 커맨드 설명입니다.</summary>
    public string Description { get; } = description;

    /// <summary>Material Symbols 아이콘 이름입니다.</summary>
    public string Icon { get; } = icon;
}
