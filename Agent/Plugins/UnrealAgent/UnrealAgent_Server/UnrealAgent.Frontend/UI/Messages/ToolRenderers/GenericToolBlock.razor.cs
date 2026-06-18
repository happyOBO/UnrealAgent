using System.Text.Json;
using Microsoft.AspNetCore.Components;
using UnrealAgent.Backend.Chat;

namespace UnrealAgent.Frontend.UI.Messages.ToolRenderers;

/// <summary>
/// 전용 렌더러가 없는 모든 도구(MCP·Glob·Grep·Read·Agent·ToolSearch 등)의 범용 렌더러입니다.
/// 입력 파라미터(JSON)와 결과를 그대로 보여 "무엇을 호출했고 어디서 멈췄는지" 디버깅을 돕습니다.
/// </summary>
public partial class GenericToolBlock
{
    /// <summary>도구 메시지입니다.</summary>
    [Parameter] public ChatUIMessage.Tool Message { get; set; } = null!;

    /// <summary>
    /// 입력 JSON을 (키, 값) 쌍 목록으로 분해합니다.
    /// 객체가 아니거나 파싱 실패 시 키 없는 단일 raw 항목으로 반환합니다.
    /// </summary>
    private IReadOnlyList<(string Key, string Value)> InputPairs()
    {
        if (string.IsNullOrEmpty(Message.Input))
            return [];

        try
        {
            using JsonDocument Doc = JsonDocument.Parse(Message.Input);
            if (Doc.RootElement.ValueKind != JsonValueKind.Object)
                return [("", Message.Input)];

            List<(string, string)> Pairs = [];
            foreach (JsonProperty Prop in Doc.RootElement.EnumerateObject())
            {
                string Value = Prop.Value.ValueKind == JsonValueKind.String
                    ? Prop.Value.GetString() ?? ""
                    : Prop.Value.GetRawText();

                Pairs.Add((Prop.Name, Truncate(Value)));
            }

            return Pairs;
        }
        catch
        {
            return [("", Message.Input)];
        }
    }

    /// <summary>긴 값은 잘라 표시합니다(디버깅 요약 목적).</summary>
    private static string Truncate(string Value, int Max = 2000)
        => Value.Length > Max ? Value[..Max] + " …(truncated)" : Value;

    /// <summary>결과를 줄 단위로 분리합니다.</summary>
    private string[] OutputLines()
        => string.IsNullOrEmpty(Message.Content)
            ? []
            : Message.Content.Split('\n', StringSplitOptions.RemoveEmptyEntries);

    /// <summary>결과 줄에 ERROR/WARN/SUCCESS 색상을 적용합니다 (CodeBlock과 동일 규칙).</summary>
    private static string FormatOutputLine(string Line)
    {
        if (Line.Contains("ERROR", StringComparison.OrdinalIgnoreCase))
            return $"<span class=\"text-[#e05e5e]\">{System.Net.WebUtility.HtmlEncode(Line)}</span>";
        if (Line.Contains("WARN", StringComparison.OrdinalIgnoreCase))
            return $"<span class=\"text-[#e5c07b]\">{System.Net.WebUtility.HtmlEncode(Line)}</span>";
        if (Line.Contains("SUCCESS", StringComparison.OrdinalIgnoreCase))
            return $"<span class=\"text-[#98c379]\">{System.Net.WebUtility.HtmlEncode(Line)}</span>";
        return $"<span class=\"text-[#aaa]\">{System.Net.WebUtility.HtmlEncode(Line)}</span>";
    }
}
