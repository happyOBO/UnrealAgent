using Microsoft.AspNetCore.Components;
using UnrealAgent.Backend.Model;
using UnrealAgent.Backend.Token;
using UnrealAgent.Frontend.Infrastructure;

namespace UnrealAgent.Frontend.UI.Input;

public partial class TokenMeter
{
    /// <summary>토큰 추적기 입니다.</summary>
    [Inject] private TokenTracker TokenTracker { get; set; } = null!;

    /// <summary>모델 설정입니다.</summary>
    [Inject] private ModelSettings ModelSettings { get; set; } = null!;
    
    /// <summary>현재 컨텍스트 토큰 수입니다. 부모에서 전달받아 변경 시 re-render를 트리거합니다.</summary>
    [Parameter] public long ContextTokens { get; set; }

    /// <summary>카테고리별 컨텍스트 사용량입니다.</summary>
    private TokenUsage Usage => TokenTracker.GetTokenUsage(ContextTokens);
    
    /// <summary>현재 컨텍스트 사용률입니다.</summary>
    private double UsagePercent => Usage.UsagePercent;
    
    /// <summary>퍼센트 텍스트 색상입니다. 40% 이하 녹색, 70% 이하 주황, 초과 시 빨강입니다.</summary>
    private string PercentColorClass => UsagePercent switch
    {
        <= 40 => $"text-[{ChatColors.Success}]",
        <= 70 => $"text-[{ChatColors.Warning}]",
        _ => $"text-[{ChatColors.Error}]"
    };

    /// <summary>70% 초과 시 pulse 애니메이션을 추가합니다.</summary>
    private string PercentAnimClass => UsagePercent > 70 ? "animate-pulse" : "";
    
    /// <summary>바 색상입니다.</summary>
    private string BarColorClass => UsagePercent switch
    {
        <= 40 => $"bg-[{ChatColors.Success}]",
        <= 70 => $"bg-[{ChatColors.Warning}]",
        _ => $"bg-[{ChatColors.Error}]"
    };

    /// <summary>바 그림자입니다.</summary>
    private string BarShadowClass => UsagePercent switch
    {
        <= 40 => "",
        <= 70 => "",
        _ => "shadow-[0_0_6px_rgba(224,94,94,0.3)]"
    };

    /// <summary>토큰 수를 축약 형식으로 표시합니다.</summary>
    private static string FormatTokens(long Tokens) => Tokens switch
    {
        >= 1_000_000 => $"{Tokens / 1_000_000.0:F1}M",
        >= 1_000 => $"{Tokens / 1_000.0:F1}k",
        _ => Tokens.ToString()
    };
    
    /// <summary>토큰 수와 컨텍스트 윈도우 대비 퍼센트를 함께 표시합니다.</summary>
    private string FormatTokensWithPct(long Tokens)
    {
        string Formatted = FormatTokens(Tokens);
        if (ModelSettings.ContextWindow <= 0) 
            return Formatted;
        
        double Percent = (double)Tokens / ModelSettings.ContextWindow * 100;
        return $"{Formatted} ({Percent:F1}%)";
    }
}