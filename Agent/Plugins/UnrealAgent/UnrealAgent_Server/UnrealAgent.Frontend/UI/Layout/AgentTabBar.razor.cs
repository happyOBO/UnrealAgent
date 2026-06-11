using Microsoft.AspNetCore.Components;
using UnrealAgent.Backend.Team;

namespace UnrealAgent.Frontend.UI.Layout;

public partial class AgentTabBar : IDisposable
{
    /// <summary>팀 정보입니다.</summary>
    [Parameter, EditorRequired] public Team Team { get; set; } = null!;
    
    /// <summary>탭 선택 시 호출됩니다. null이면 리더, 값이면 팀원 포트입니다.</summary>
    [Parameter] public EventCallback<int?> OnTabSelected { get; set; }
    
    /// <summary>현재 선택된 포트입니다. null이면 리더 탭입니다.</summary>
    private int? SelectedPort;
    
    protected override void OnInitialized()
    {
        Team.OnTeamChanged += OnTeamChanged;
    }

    public void Dispose()
    {
        Team.OnTeamChanged -= OnTeamChanged;
    }
    
    private void OnTeamChanged() => InvokeAsync(StateHasChanged);
    
    /// <summary>탭을 선택하고 부모에게 알립니다.</summary>
    private void SelectTab(int? Port)
    {
        SelectedPort = Port;
        OnTabSelected.InvokeAsync(Port);
    }
    
    /// <summary>리더 탭의 CSS 클래스를 반환합니다.</summary>
    private string LeaderTabClass()
    {
        string Base = "h-full flex items-center gap-2 px-5 border-t-2 text-[11px] font-semibold tracking-wide transition-colors relative";
        return SelectedPort is null
            ? $"{Base} border-t-transparent border-b border-b-[#0070e0] bg-[#1a1a1a] text-[#fff]"
            : $"{Base} border-t-transparent border-b border-b-transparent text-[#888] hover:text-[#ccc] hover:bg-[#1a1a1a]/50";
    }

    /// <summary>팀원 탭의 CSS 클래스를 반환합니다.</summary>
    private string TeammateTabClass(int Port)
    {
        string Base = "h-full flex items-center gap-2 px-4 border-t-2 text-[11px] font-semibold tracking-wide transition-colors relative";
        return SelectedPort == Port
            ? $"{Base} border-t-transparent border-b border-b-[#10b981] bg-[#1a1a1a] text-[#fff]"
            : $"{Base} border-t-transparent border-b border-b-transparent text-[#888] hover:text-[#ccc] hover:bg-[#1a1a1a]/50";
    }
}