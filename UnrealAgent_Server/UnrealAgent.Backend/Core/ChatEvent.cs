namespace UnrealAgent.Backend.Core;

/// <summary>
/// 에이전트에서 UI로 전달되는 스트리밍 이벤트입니다.
/// </summary>
public abstract record ChatEvent
{
    /// <summary>Claude의 텍스트 응답입니다.</summary>
    public sealed record Text(string Content) : ChatEvent;
    
    /// <summary>Claude의 사고 과정(Extended Thinking) 응답입니다.</summary>
    public sealed record Thinking(string Content) : ChatEvent;
    
    /// <summary>스트림 종료입니다.</summary>
    public sealed record Done : ChatEvent;
}