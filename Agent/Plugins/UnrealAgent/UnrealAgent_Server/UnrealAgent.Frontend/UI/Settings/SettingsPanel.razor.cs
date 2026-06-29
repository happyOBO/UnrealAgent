using Microsoft.AspNetCore.Components;

namespace UnrealAgent.Frontend.UI.Settings;

public partial class SettingsPanel
{
    /// <summary>패널 표시 여부입니다.</summary>
    [Parameter] public bool bIsVisible { get; set; }

    /// <summary>패널 닫기 콜백입니다.</summary>
    [Parameter] public EventCallback OnClose { get; set; }
}
