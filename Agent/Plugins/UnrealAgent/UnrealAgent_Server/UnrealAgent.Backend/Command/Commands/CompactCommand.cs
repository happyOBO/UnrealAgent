using UnrealAgent.Backend.Agent;
using UnrealAgent.Backend.Chat;
using UnrealAgent.Backend.Command.Attributes;
using UnrealAgent.Backend.Conversation;

namespace UnrealAgent.Backend.Command.Commands;

/// <summary>
/// 대화 컨텍스트를 압축하는 슬래시 커맨드입니다.
/// claude CLI 내장 /compact에 위임합니다. 압축 결과는 CLI 세션 파일에 저장되어
/// 다음 턴의 --resume에서 그대로 이어집니다.
/// </summary>
[AgentCommand("/compact", "컨텍스트를 요약하여 압축합니다. 지시사항을 추가할 수 있습니다", icon: "compress")]
public class CompactCommand(AgentLoop Loop) : IAgentCommand
{
    public async IAsyncEnumerable<ChatEvent> ExecuteAsync(string[] Args, AgentSession Session)
    {
        // 이어갈 CLI 세션이 없으면 압축할 대화도 없습니다.
        if (Session.ClaudeSessionId is null)
        {
            yield return new ChatEvent.System("압축할 대화가 없습니다.");
            yield break;
        }

        yield return new ChatEvent.System("컨텍스트 윈도우를 요약하고 있습니다.");

        // CLI 내장 /compact 호출. bCliCommand로 해당 턴만 슬래시 커맨드 격리를 해제합니다.
        string Text = Args.Length > 0 ? $"/compact {string.Join(' ', Args)}" : "/compact";
        UserInput Input = new(Text) { bCliCommand = true };

        bool bFailed = false;

        await foreach (ChatEvent Evt in Loop.RunAsync(Input, Session))
        {
            switch (Evt)
            {
                // 커맨드는 Done을 발행하지 않는 규약이므로 삼킵니다.
                case ChatEvent.Done:
                    break;

                case ChatEvent.System:
                    bFailed = true;
                    yield return Evt;
                    break;

                default:
                    yield return Evt;
                    break;
            }
        }

        if (!bFailed)
            yield return new ChatEvent.System("컨텍스트 압축이 완료되었습니다.");
    }
}
