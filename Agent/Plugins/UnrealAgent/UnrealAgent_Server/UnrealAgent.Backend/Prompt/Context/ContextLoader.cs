using System.Text.RegularExpressions;
using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

namespace UnrealAgent.Backend.Prompt.Context;

/// <summary>
/// 컨텍스트 .md 파일을 파싱하여 ContextDocument를 생성합니다.
/// SkillLoader와 동일하게 YAML 프론트매터(--- 사이)를 파싱하고 본문을 분리합니다.
/// </summary>
public static partial class ContextLoader
{
    /// <summary>YAML 프론트매터를 추출하는 정규식입니다.</summary>
    [GeneratedRegex(@"^---\s*\n([\s\S]*?)---\s*\n?", RegexOptions.None)]
    private static partial Regex FrontmatterRegex();

    /// <summary>YAML 디시리얼라이저입니다. kebab-case 키를 지원합니다.</summary>
    private static readonly IDeserializer YamlDeserializer = new DeserializerBuilder()
        .WithNamingConvention(HyphenatedNamingConvention.Instance)
        .IgnoreUnmatchedProperties()
        .Build();

    /// <summary>
    /// 컨텍스트 파일 경로에서 ContextDocument를 파싱합니다.
    /// 이름/본문/키워드가 비어 있으면 null을 반환합니다.
    /// </summary>
    public static ContextDocument? Load(string FilePath)
    {
        if (!File.Exists(FilePath))
            return null;

        string RawContent = File.ReadAllText(FilePath);
        ContextFrontmatter Frontmatter = ParseFrontmatter(RawContent, out string Body);

        string Name = Frontmatter.Name ?? Path.GetFileNameWithoutExtension(FilePath);
        Body = Body.Trim();

        // 키워드를 소문자로 정규화합니다 (매칭은 소문자 기준).
        List<string> Keywords = (Frontmatter.Keywords ?? [])
            .Select(K => K.Trim().ToLowerInvariant())
            .Where(K => K.Length > 0)
            .ToList();

        if (Body.Length == 0 || Keywords.Count == 0)
            return null;

        return new ContextDocument
        {
            Name = Name,
            Description = Frontmatter.Description ?? string.Empty,
            Keywords = Keywords,
            Content = Body,
        };
    }

    /// <summary>YAML 프론트매터를 파싱하고 본문을 분리합니다.</summary>
    private static ContextFrontmatter ParseFrontmatter(string RawContent, out string Body)
    {
        Match Match = FrontmatterRegex().Match(RawContent);
        if (!Match.Success)
        {
            Body = RawContent;
            return new ContextFrontmatter();
        }

        Body = RawContent[Match.Length..];
        string Yaml = Match.Groups[1].Value;

        try
        {
            return YamlDeserializer.Deserialize<ContextFrontmatter>(Yaml);
        }
        catch
        {
            return new ContextFrontmatter();
        }
    }
}

//-----------------------------------------------------------------------------
// ContextFrontmatter
//-----------------------------------------------------------------------------

/// <summary>
/// 컨텍스트 .md YAML 프론트매터의 역직렬화 대상 클래스입니다.
/// </summary>
internal sealed class ContextFrontmatter
{
    public string? Name { get; set; }
    public string? Description { get; set; }
    public List<string>? Keywords { get; set; }
}
