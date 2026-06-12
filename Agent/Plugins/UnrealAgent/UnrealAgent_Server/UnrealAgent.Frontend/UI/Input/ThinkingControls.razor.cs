using Microsoft.AspNetCore.Components;
using UnrealAgent.Backend.Model;

namespace UnrealAgent.Frontend.UI.Input;

/// <summary>
/// ThinkingToggle과 EffortSelector를 묶는 컨테이너입니다.
/// 모델이 두 컨트롤 모두 지원하지 않으면 컨테이너 자체를 숨깁니다.
/// </summary>
public partial class ThinkingControls : IDisposable
{
    /// <summary>모델 설정 서비스입니다.</summary>
    [Inject] private ModelSettings Settings { get; set; } = null!;

    /// <summary>Thinking 토글 표시 여부입니다.</summary>
    private bool bShowThinking => Settings.ThinkingSupport == ThinkingSupport.Toggle;

    /// <summary>Effort 선택 표시 여부입니다.</summary>
    private bool bShowEffort => Settings.bSupportsEffort;

    /// <summary>모델 변경 시 가시성을 갱신하기 위해 설정 변경을 구독합니다.</summary>
    protected override void OnInitialized() => Settings.OnChanged += HandleSettingsChanged;

    public void Dispose() => Settings.OnChanged -= HandleSettingsChanged;

    private void HandleSettingsChanged() => InvokeAsync(StateHasChanged);
}
