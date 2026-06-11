using System.Diagnostics;
using Microsoft.Extensions.Hosting;
using UnrealAgent.Backend.Chat;
using UnrealAgent.Backend.Conversation;
using UnrealAgent.Backend.Core;
using UnrealAgent.Backend.Team;

namespace UnrealAgent.Backend.Agent;

/// <summary>
/// 메시지 큐 + 에이전트 호출 + ChatStore 관리를 담당하는 서비스입니다.
/// 리더와 팀원 모두 동일한 서비스를 사용합니다.
/// Store 수정은 직접 하지 않고 OnChatEvent를 통해 UI 스레드에서 실행합니다.
/// </summary>
public sealed class AgentRunner(AgentSession Session, IHostApplicationLifetime Lifetime) : BackgroundService
{
    /// <summary>반응형 상태 관리자입니다.</summary>
    public ChatStore Store { get; } = new();
    
    /// <summary>사용자 입력을 순서대로 보관하는 메시지 큐입니다.</summary>
    private readonly Queue<UserInput> MessageQueue = new();

    /// <summary>큐에 메시지가 도착하면 BackgroundService 루프를 깨우는 시그널입니다.</summary>
    private readonly SemaphoreSlim Signal = new(0);
    
    /// <summary>ChatEvent 발생 시 UI 스레드에서 처리할 콜백입니다. 최신 구독자만 유지합니다.</summary>
    public Func<ChatEvent, Task>? OnChatEvent { get; set; }
    
    protected override async Task ExecuteAsync(CancellationToken Ct)
    {
        while (!Ct.IsCancellationRequested)
        {
            // 시그널 대기 — 사용자 입력 시 즉시, 아니면 3초마다 메일박스 확인
            await Task.WhenAny(Signal.WaitAsync(Ct), Task.Delay(3000, Ct));
            
            // 부모 프로세스 생존 확인 (팀원일 때만)
            CheckParentAlive();
            
            // 메일박스 폴링 (팀 모드일 때만)
            await DrainMailbox();
            
            // 큐에 아무것도 없으면 다시 대기
            if (MessageQueue.Count == 0) 
                continue;
            
            // 메시지 큐를 순차 처리합니다.
            await DrainQueue();
        }
    }
    
    /// <summary>
    /// 메시지를 큐에 추가하고 BackgroundService 루프를 깨웁니다.
    /// </summary>
    public async Task EnqueueMessage(UserInput Input)
    {
        // 사용자 메세지 UI를 위해 추가
        await DispatchEventAsync(new ChatEvent.User(Input.Text, Input.ImageMediaType, Input.ImageBase64));
        
        MessageQueue.Enqueue(Input);
        Signal.Release();
    }
    
    /// <summary>
    /// 큐에서 메시지를 하나씩 꺼내 순차적으로 처리합니다.
    /// </summary>
    private async Task DrainQueue()
    {
        while (MessageQueue.TryDequeue(out UserInput? Input))
        {
            try
            {
                await foreach (ChatEvent Evt in Session.ProcessMessage(Input))
                {
                    await DispatchEventAsync(Evt);
                }
            }
            catch (Exception e)
            {
                await DispatchEventAsync(new ChatEvent.System(e.Message));
            }
        }
    }
    
    /// <summary>
    /// ChatEvent를 UI 스레드로 디스패치합니다.
    /// </summary>
    private Task DispatchEventAsync(ChatEvent Evt)
    {
        if (OnChatEvent is { } Handler)
            return Handler(Evt);

        // 구독자가 없으면 (UI 미로드 상태) 직접 Store에 누적
        Store.Process(Evt);
        return Task.CompletedTask;
    }
    
    /// <summary>
    /// 메일박스에서 메시지를 읽어 큐에 추가합니다.
    /// </summary>
    private async Task DrainMailbox()
    {
        if (Session.Team.TeamName is null)
            return;

        string MailboxDir = AgentPaths.GetMailboxDir(Session.Team.TeamName);
        List<TeamMessage> Messages = await Mailbox.TakeAllAsync(MailboxDir, Session.Team.AgentName);

        foreach (TeamMessage Msg in Messages)
        {
            if (Msg is { Type: MessageType.Command, Content: "shutdown" })
            {
                Lifetime.StopApplication();
                return;
            }

            MessageQueue.Enqueue(new UserInput($"[{Msg.From}]: {Msg.Content}"));
        }
    }
    
    /// <summary>
    /// 부모 프로세스가 종료되었으면 자신도 종료합니다.
    /// </summary>
    private void CheckParentAlive()
    {
        if (Session.Team.ParentPid is not { } Pid)
            return;

        try
        {
            Process.GetProcessById(Pid);
        }
        catch (ArgumentException)
        {
            Lifetime.StopApplication();
        }
    }
}