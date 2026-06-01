namespace UnrealAgent.Backend.Chat;

/// <summary>
/// UI에 표시할 채팅 메시지 저장소입니다.
/// ChatEvent를 처리하여 ChatUIMessage 목록을 구성합니다.
/// </summary>
public sealed class ChatStore 
{
    /// <summary>UI에 표시되는 채팅 메시지 목록입니다.</summary>
    public List<ChatUIMessage> Messages { get; } = [];
    
    /// <summary>유저가 메시지를 보낸 뒤, AI 응답이 도착하기 전까지 true입니다.</summary>
    public bool bIsReceiving { get; private set; }

    /// <summary>
    /// ChatEvent를 처리하여 UI 메시지를 업데이트합니다.
    /// </summary>
    public void Process(ChatEvent Evt)
    {
        bIsReceiving = Evt is ChatEvent.User;

        switch (Evt)
        {
            case ChatEvent.User { Content: var Content }:
            {
                Messages.Add(new ChatUIMessage.User(Content));

                break;
            }

            case ChatEvent.Assistant { Content: var Content }:
            {
                ThinkingComplete();
                
                if (Messages.Count > 0 && Messages[^1] is ChatUIMessage.Assistant)
                {
                    Messages[^1] = Messages[^1].Append(Content);
                }
                else
                {
                    Messages.Add(new ChatUIMessage.Assistant(Content));
                }
                
                break;
            }
                
            case ChatEvent.Thinking { Content: var Content }:
            {
                if (Messages.Count > 0 && Messages[^1] is ChatUIMessage.Thinking { bIsCompleted: false })
                {
                    Messages[^1] = Messages[^1].Append(Content);
                }
                else
                {
                    Messages.Add(new ChatUIMessage.Thinking(Content)
                    {
                        StartTime = DateTime.Now,
                        bIsCompleted = false
                    });
                }

                break;
            }

            case ChatEvent.System { Content: var Content }:
            {
                Messages.Add(new ChatUIMessage.System(Content));
                break;
            }

            case ChatEvent.Done:
            {
                ThinkingComplete();

                break;
            }
        }
    }

    /// <summary>미완료 Thinking 메시지를 완료 처리합니다.</summary>
    private void ThinkingComplete()
    {
        if (Messages.Count > 0 && Messages[^1] is ChatUIMessage.Thinking { bIsCompleted: false } T)
        {
            Messages[^1] = T with
            {
                ElapsedSeconds = (DateTime.Now - T.StartTime).TotalSeconds,
                bIsCompleted = true
            };
        }
    }
}