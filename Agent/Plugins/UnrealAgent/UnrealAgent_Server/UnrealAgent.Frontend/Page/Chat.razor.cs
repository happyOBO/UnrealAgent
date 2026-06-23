using Microsoft.AspNetCore.Components;
using UnrealAgent.Backend.Agent;
using UnrealAgent.Backend.Chat;
using UnrealAgent.Backend.Conversation;
using UnrealAgent.Backend.Core;
using UnrealAgent.Backend.Security;

namespace UnrealAgent.Frontend.Page;

public partial class Chat : IAsyncDisposable
{
    /// <summary>에이전트 실행 서비스입니다.</summary>
    [Inject] private AgentRunner AgentRunner { get; set; } = null!;

    /// <summary>에이전트 세션입니다.</summary>
    [Inject] private AgentSession AgentSession { get; set; } = null!;

    /// <summary>세션 영속화 저장소입니다.</summary>
    [Inject] private SessionStore SessionStore { get; set; } = null!;

    /// <summary>저장된 세션이 있고 현재 대화가 비어 있을 때만 복원할 수 있습니다.</summary>
    private bool CanRestore => SessionStore.HasSaved() && AgentRunner.Store.Messages.Count == 0;

    /// <summary>새 세션을 시작합니다. 기존 /clear 로직(대화·세션·저장 파일 초기화)을 재사용합니다.</summary>
    private async Task NewSession()
    {
        await AgentRunner.EnqueueMessage(new UserInput("/clear"));
    }

    /// <summary>저장된 세션을 복원합니다. CLI 세션 ID는 --resume으로, UI 메시지는 화면에 재표시합니다.</summary>
    private void RestoreContext()
    {
        if (SessionStore.Load() is not { } Saved)
            return;

        AgentSession.ClaudeSessionId = Saved.ClaudeSessionId;
        AgentSession.Mode = Saved.Mode;
        AgentRunner.Store.Restore(Saved.Messages);

        // dev-block 모드면, 다음 첫 턴에 AgentLoop가 재개-넛지를 주입하도록 1회 플래그를 켭니다.
        if (AppSettings.IsDevBlockModeEnabled())
            AgentSession.bResumeCheckPending = true;

        StateHasChanged();
    }

    /// <summary>설정 패널 표시 여부입니다.</summary>
    private bool bShowSettings;

    /// <summary>설정 패널을 토글합니다.</summary>
    private void ToggleSettings() => bShowSettings = !bShowSettings;

    /// <summary>플랜 사용량 패널 표시 여부입니다.</summary>
    private bool bShowUsage;

    /// <summary>플랜 사용량 패널을 토글합니다.</summary>
    private void ToggleUsage() => bShowUsage = !bShowUsage;

    /// <summary>현재 대기 중인 권한 요청입니다.</summary>
    private ChatEvent.ToolPermissionRequest? PendingPermission;
    
    /// <summary>선택된 팀원 포트입니다. null이면 리더 탭입니다.</summary>
    private int? SelectedTeammatePort;

    protected override void OnInitialized()
    {
        AgentRunner.OnChatEvent = OnChatEvent;
        AgentRunner.OnBusyChanged = OnBusyChanged;
    }

    public ValueTask DisposeAsync()
    {
        if (AgentRunner.OnChatEvent == OnChatEvent)
            AgentRunner.OnChatEvent = null;

        if (AgentRunner.OnBusyChanged == OnBusyChanged)
            AgentRunner.OnBusyChanged = null;

        return ValueTask.CompletedTask;
    }

    /// <summary>busy 상태 변화 시 UI 스레드에서 재렌더하여 입력 가드/Stop 버튼을 갱신합니다.</summary>
    private Task OnBusyChanged() => InvokeAsync(StateHasChanged);

    /// <summary>
    /// AgentRunner의 ChatEvent를 UI 스레드에서 처리합니다.
    /// Store 수정과 렌더링이 같은 스레드에서 실행되어 스레드 안전성을 보장합니다.
    /// </summary>
    private Task OnChatEvent(ChatEvent Evt) => InvokeAsync(() =>
    {
        if (Evt is ChatEvent.ToolPermissionRequest Req)
            PendingPermission = Req;
        else
            AgentRunner.Store.Process(Evt);

        StateHasChanged();
    });

    /// <summary>권한 다이얼로그에서 사용자가 결정했을 때 호출됩니다.</summary>
    private void HandlePermissionDecision(ToolPermission Decision)
    {
        PendingPermission?.Tcs.TrySetResult(Decision);
        PendingPermission = null;
    }
}