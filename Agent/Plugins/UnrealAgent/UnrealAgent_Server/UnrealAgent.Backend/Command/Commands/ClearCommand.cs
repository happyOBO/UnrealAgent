using UnrealAgent.Backend.Agent;
using UnrealAgent.Backend.Chat;
using UnrealAgent.Backend.Command.Attributes;
using UnrealAgent.Backend.Core;

namespace UnrealAgent.Backend.Command.Commands;

/// <summary>
/// 대화 내역을 초기화하는 슬래시 커맨드입니다.
/// </summary>
[AgentCommand("/clear", "대화 내역을 초기화합니다", icon: "restart_alt")]
public class ClearCommand(SessionStore SessionStore) : IAgentCommand
{
    /// <summary>
    /// 대화 히스토리와 UI 메시지를 모두 초기화합니다.
    /// </summary>
    public async IAsyncEnumerable<ChatEvent> ExecuteAsync(string[] Args, AgentSession Session)
    {
        Session.Conversation.Clear();

        // claude CLI 세션을 끊어 다음 턴이 새 세션으로 시작되도록 합니다.
        Session.ClaudeSessionId = null;

        // 디스크에 저장된 세션도 삭제하여 복원 대상이 남지 않도록 합니다.
        SessionStore.Delete();

        yield return new ChatEvent.Command("clear", "");
    }
}