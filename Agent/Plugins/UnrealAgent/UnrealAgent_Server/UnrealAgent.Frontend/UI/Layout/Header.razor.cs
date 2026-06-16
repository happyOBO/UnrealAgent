using Microsoft.AspNetCore.Components;

namespace UnrealAgent.Frontend.UI.Layout;

public partial class Header
{
    /// <summary>설정 버튼 클릭 콜백입니다.</summary>
    [Parameter] public EventCallback OnSettingsClick { get; set; }

    /// <summary>새 세션 버튼 클릭 콜백입니다.</summary>
    [Parameter] public EventCallback OnNewSession { get; set; }

    /// <summary>세션 복원 버튼 클릭 콜백입니다.</summary>
    [Parameter] public EventCallback OnRestoreContext { get; set; }

    /// <summary>저장된 세션이 있어 복원 버튼을 활성화할지 여부입니다.</summary>
    [Parameter] public bool bCanRestore { get; set; }
}