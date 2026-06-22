using System.Text;
using UnrealAgent.Backend.Core;

namespace UnrealAgent.Backend.Prompt.Context;

/// <summary>
/// 도메인별 UE Python API 컨텍스트 문서를 발견하고, 사용자 메시지의 키워드에
/// 맞는 문서를 골라 user 턴에 주입할 <system-reminder> 블록을 생성합니다.
///
/// UnrealClaude의 context-loader.js(키워드 매칭 자동 주입)를 C# 측에서 재현한 것으로,
/// 시스템 프롬프트가 아니라 user 메시지에 주입하여 프롬프트 캐시를 무효화하지 않습니다.
/// </summary>
public sealed class ContextRegistry
{
    /// <summary>발견된 컨텍스트 문서 목록입니다.</summary>
    private readonly List<ContextDocument> Contexts = [];

    /// <summary>한 번에 주입할 최대 도메인 수입니다 (토큰 예산 보호).</summary>
    private const int MaxInjectedDomains = 3;

    /// <summary>
    /// 컨텍스트 디렉토리를 스캔하여 .md 문서를 로딩합니다. 시작 시 1회 호출합니다.
    /// </summary>
    public void DiscoverContexts()
    {
        if (!Directory.Exists(AgentPaths.ContextsDir))
            return;

        foreach (string FilePath in Directory.GetFiles(AgentPaths.ContextsDir, "*.md"))
        {
            ContextDocument? Doc = ContextLoader.Load(FilePath);
            if (Doc is not null)
                Contexts.Add(Doc);
        }
    }

    /// <summary>
    /// 사용자 텍스트의 키워드와 매칭되는 컨텍스트를 골라 주입용 블록을 생성합니다.
    /// 매칭된 키워드 수가 많은 도메인 우선, 최대 MaxInjectedDomains개까지 결합합니다.
    /// 매칭이 없으면 null을 반환합니다.
    /// </summary>
    public string? MatchByKeywords(string? UserText)
    {
        if (string.IsNullOrWhiteSpace(UserText) || Contexts.Count == 0)
            return null;

        string Lower = UserText.ToLowerInvariant();

        List<ContextDocument> Matched = Contexts
            .Select(C => (Doc: C, Hits: C.Keywords.Count(K => Lower.Contains(K))))
            .Where(X => X.Hits > 0)
            .OrderByDescending(X => X.Hits)
            .Take(MaxInjectedDomains)
            .Select(X => X.Doc)
            .ToList();

        if (Matched.Count == 0)
            return null;

        StringBuilder Sb = new();
        Sb.AppendLine("<system-reminder>");
        Sb.AppendLine(
            "Relevant Unreal Engine Python API context for this request is provided below. " +
            "Prefer these patterns when writing `unreal` module code via execute_python.");

        foreach (ContextDocument Doc in Matched)
        {
            Sb.AppendLine();
            Sb.AppendLine($"# UE Context: {Doc.Name}");
            Sb.AppendLine(Doc.Content);
        }

        Sb.Append("</system-reminder>");
        return Sb.ToString();
    }
}
