namespace UnrealAgent.Frontend.Page;

public partial class Chat
{
    /// <summary>설정 패널 표시 여부입니다.</summary>
    private bool bShowSettings;

    /// <summary>설정 패널을 토글합니다.</summary>
    private void ToggleSettings() => bShowSettings = !bShowSettings;
}