namespace UnrealAgent.Backend.Team;

//-----------------------------------------------------------------------------
// MessageType
//-----------------------------------------------------------------------------

/// <summary>팀 메시지의 종류입니다.</summary>
public enum MessageType
{
    /// <summary>일반 대화 메시지입니다.</summary>
    Chat,

    /// <summary>제어 명령입니다 (shutdown 등).</summary>
    Command
}

//-----------------------------------------------------------------------------
// TeamMessage
//-----------------------------------------------------------------------------

/// <summary>팀원 간 주고받는 메시지입니다.</summary>
public record TeamMessage(string From, MessageType Type, string Content, DateTime Timestamp);