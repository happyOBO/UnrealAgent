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
public sealed class AgentRunner(AgentSession Session, SessionStore SessionStore, IHostApplicationLifetime Lifetime) : BackgroundService
{
    /// <summary>메일박스/부모 생존을 확인하는 폴링 주기(ms)입니다.</summary>
    private const int MailboxPollMs = 3000;

    /// <summary>반응형 상태 관리자입니다.</summary>
    public ChatStore Store { get; } = new();
    
    /// <summary>사용자 입력을 순서대로 보관하는 메시지 큐입니다.</summary>
    private readonly Queue<UserInput> MessageQueue = new();

    /// <summary>큐에 메시지가 도착하면 BackgroundService 루프를 깨우는 시그널입니다.</summary>
    private readonly SemaphoreSlim Signal = new(0);
    
    /// <summary>ChatEvent 발생 시 UI 스레드에서 처리할 콜백입니다. 최신 구독자만 유지합니다.</summary>
    public Func<ChatEvent, Task>? OnChatEvent { get; set; }

    /// <summary>busy 상태가 바뀔 때 UI 스레드에서 재렌더하기 위한 콜백입니다.</summary>
    public Func<Task>? OnBusyChanged { get; set; }

    /// <summary>턴이 진행 중인지 여부입니다. true이면 새 메시지 전송을 막고 Stop 버튼을 노출합니다.</summary>
    public bool bIsBusy { get; private set; }

    /// <summary>
    /// 마지막 턴 기준 입력측 컨텍스트 토큰 사용량입니다. TokenMeter가 이 값을 표시합니다.
    /// CLI(Claude Code) 경로는 Conversation 히스토리를 채우지 않으므로 여기서 직접 추적합니다.
    /// </summary>
    public long ContextTokens { get; private set; }

    /// <summary>현재 진행 중인 턴의 취소 토큰 소스입니다. Stop/Esc 시 Cancel합니다.</summary>
    private CancellationTokenSource? TurnCts;

    /// <summary>현재 진행 중인 턴을 취소합니다. UI의 Stop 버튼/Esc 키에서 호출됩니다.</summary>
    public void CancelCurrentTurn() => TurnCts?.Cancel();

    protected override async Task ExecuteAsync(CancellationToken Ct)
    {
        while (!Ct.IsCancellationRequested)
        {
            // 시그널 대기 — 사용자 입력 시 즉시, 아니면 폴링 주기마다 메일박스 확인
            await Task.WhenAny(Signal.WaitAsync(Ct), Task.Delay(MailboxPollMs, Ct));
            
            // 부모 프로세스 생존 확인 (팀원일 때만)
            CheckParentAlive();
            
            // 메일박스 폴링 (팀 모드일 때만)
            await DrainMailbox();
            
            // 큐에 아무것도 없으면 다시 대기
            if (MessageQueue.Count == 0)
                continue;

            // 메시지 큐를 순차 처리합니다.
            await DrainQueue(Ct);
        }
    }
    
    /// <summary>
    /// 메시지를 큐에 추가하고 BackgroundService 루프를 깨웁니다.
    /// </summary>
    public async Task EnqueueMessage(UserInput Input)
    {
        // 이미 턴이 진행 중이면 무시합니다(UI 가드의 방어선). 진행 중에는 새 입력을 받지 않습니다.
        if (bIsBusy)
            return;

        // 전송 즉시 busy로 전환하여 추가 전송을 막고 Stop 버튼을 노출합니다.
        await SetBusyAsync(true);

        // 사용자 메세지 UI를 위해 추가
        await DispatchEventAsync(new ChatEvent.User(Input.Text, Input.ImageMediaType, Input.ImageBase64));

        MessageQueue.Enqueue(Input);
        Signal.Release();
    }

    /// <summary>
    /// 토큰 사용량/초기화 이벤트를 보고 컨텍스트 토큰 표시값을 갱신합니다.
    /// 갱신 후 이어지는 DispatchEventAsync의 StateHasChanged가 미터를 다시 그립니다.
    /// </summary>
    private void UpdateContextTokens(ChatEvent Evt)
    {
        switch (Evt)
        {
            case ChatEvent.Usage Usage:
                ContextTokens = Usage.ContextTokens;
                break;

            // /clear는 세션을 끊으므로 컨텍스트도 0으로 되돌립니다.
            case ChatEvent.Command { Name: var Name } when Name.Equals("clear", StringComparison.OrdinalIgnoreCase):
                ContextTokens = 0;
                break;
        }
    }

    /// <summary>busy 상태를 변경하고 UI에 통지합니다.</summary>
    private async Task SetBusyAsync(bool Value)
    {
        if (bIsBusy == Value)
            return;

        bIsBusy = Value;

        if (OnBusyChanged is { } Handler)
            await Handler();
    }

    /// <summary>
    /// 큐에서 메시지를 하나씩 꺼내 순차적으로 처리합니다.
    /// </summary>
    private async Task DrainQueue(CancellationToken StoppingToken)
    {
        // 메일박스 등 EnqueueMessage를 거치지 않은 경로도 busy로 표시합니다.
        await SetBusyAsync(true);

        try
        {
            while (MessageQueue.TryDequeue(out UserInput? Input))
            {
                // 턴 단위 취소 토큰: 종료 토큰과 연동하여 앱 종료 시에도 취소됩니다.
                using CancellationTokenSource Cts = CancellationTokenSource.CreateLinkedTokenSource(StoppingToken);
                TurnCts = Cts;

                bool bCancelled = false;

                try
                {
                    await foreach (ChatEvent Evt in Session.ProcessMessage(Input, Cts.Token))
                    {
                        UpdateContextTokens(Evt);
                        await DispatchEventAsync(Evt);
                    }

                    // CLI가 ReadLine 취소 시 깔끔히 끝나는 경로(예외 없음)도 중단으로 간주합니다.
                    bCancelled = Cts.IsCancellationRequested;
                }
                catch (OperationCanceledException) when (Cts.IsCancellationRequested && !StoppingToken.IsCancellationRequested)
                {
                    // 권한 대기/stdin 쓰기 도중 취소된 경로: 턴 종료 이벤트로 UI 상태를 마무리합니다.
                    bCancelled = true;
                    await DispatchEventAsync(new ChatEvent.Done());
                }
                catch (Exception e)
                {
                    await DispatchEventAsync(new ChatEvent.System(e.Message));
                }
                finally
                {
                    TurnCts = null;
                }

                if (bCancelled)
                {
                    await DispatchEventAsync(new ChatEvent.System("응답이 중단되었습니다."));

                    // 사용자가 중단했으므로 대기 중인 후속 메시지도 비웁니다.
                    MessageQueue.Clear();
                }

                // 턴 종료 시점에 세션을 디스크에 저장하여 다음 실행에서 복원할 수 있게 합니다.
                PersistSession();
            }
        }
        finally
        {
            await SetBusyAsync(false);
        }
    }

    /// <summary>
    /// 현재 세션(CLI 세션 ID + UI 메시지)을 디스크에 저장합니다.
    /// CLI 세션이 없거나(예: /clear 직후) 표시할 메시지가 없으면 저장하지 않습니다.
    /// </summary>
    private void PersistSession()
    {
        if (Session.ClaudeSessionId is null || Store.Messages.Count == 0)
            return;

        SessionStore.Save(new PersistedSession
        {
            ClaudeSessionId = Session.ClaudeSessionId,
            Mode = Session.Mode,
            Messages = Store.Messages.ToList(),
            SavedAt = DateTime.UtcNow,
        });
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