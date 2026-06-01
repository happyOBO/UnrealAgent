namespace UnrealAgent.Backend.Chat;

/// <summary>
/// UI에 표시되는 채팅 메시지입니다.
/// </summary>
public abstract record ChatUIMessage
{
    /// <summary>메시지 본문입니다.</summary>
    public abstract string Content { get; init; }
    
    /// <summary>메시지 생성 시간입니다.</summary>
    public DateTime Timestamp { get; init; } = DateTime.Now;
    
    /// <summary>Content 뒤에 텍스트를 이어붙인 새 인스턴스를 반환합니다.</summary>
    public ChatUIMessage Append(string Text) => this with { Content = Content + Text };

    //-----------------------------------------------------------------------------
    // User
    //-----------------------------------------------------------------------------

    /// <summary>사용자 메시지입니다.</summary>
    public sealed record User(string Content) : ChatUIMessage
    {
        
    }
    
    //-----------------------------------------------------------------------------
    // Assistant
    //-----------------------------------------------------------------------------

    /// <summary>어시스턴트(AI) 응답입니다.</summary>
    public sealed record Assistant(string Content) : ChatUIMessage;
    
    //-----------------------------------------------------------------------------
    // Thinking
    //-----------------------------------------------------------------------------

    /// <summary>사고 과정(Extended Thinking) 메시지입니다.</summary>
    public sealed record Thinking(string Content) : ChatUIMessage
    {
        /// <summary>사고 시작 시간입니다. UI에서 실시간 경과 시간을 계산합니다.</summary>
        public DateTime StartTime { get; init; }

        /// <summary>사고 과정에 소요된 최종 시간(초)입니다. 완료 후 확정됩니다.</summary>
        public double ElapsedSeconds { get; init; }

        /// <summary>완료 여부입니다.</summary>
        public bool bIsCompleted { get; init; }
    }
    
    //-----------------------------------------------------------------------------
    // System
    //-----------------------------------------------------------------------------

    /// <summary>시스템 메시지입니다.</summary>
    public sealed record System(string Content) : ChatUIMessage;
}