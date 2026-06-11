using System.Text.RegularExpressions;
using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

namespace UnrealAgent.Backend.Skill;

/// <summary>
/// SKILL.md 파일을 파싱하여 SkillDefinition을 생성합니다.
/// YAML 프론트매터(--- 사이)를 YamlDotNet으로 파싱하고 마크다운 본문을 분리합니다.
/// </summary>
public static partial class SkillLoader
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
    /// SKILL.md 파일 경로에서 SkillDefinition을 파싱합니다.
    /// </summary>
    public static SkillDefinition? Load(string FilePath)
    {
        if (!File.Exists(FilePath))
            return null;
        
        string RawContent = File.ReadAllText(FilePath);
        string SkillRoot = Path.GetDirectoryName(FilePath)!;
        
        // 프론트매터와 본문을 분리합니다
        SkillFrontmatter Frontmatter = ParseFrontmatter(RawContent, out string Body);
        
        string Name = Frontmatter.Name!;
        string Description = Frontmatter.Description!;
        
        SkillDefinition Definition = new()
        {
            Name = Name,
            Description = Description,
            bDisableModelInvocation = Frontmatter.DisableModelInvocation,
            bUserInvocable = Frontmatter.UserInvocable,
            Content = Body.Trim(),
            SkillRoot = SkillRoot,
        };
        
        return Definition;
    }

    /// <summary>YAML 프론트매터를 파싱하고 본문을 분리합니다.</summary>
    private static SkillFrontmatter ParseFrontmatter(string RawContent, out string Body)
    {
        Match Match = FrontmatterRegex().Match(RawContent);
        if (!Match.Success)
        {
            Body = RawContent;
            return new SkillFrontmatter();
        }
        
        Body = RawContent[Match.Length..];
        string Yaml = Match.Groups[1].Value;
        
        try
        {
            return YamlDeserializer.Deserialize<SkillFrontmatter>(Yaml);
        }
        catch
        {
            return new SkillFrontmatter();
        }
    }
}

//-----------------------------------------------------------------------------
// SkillFrontmatter
//-----------------------------------------------------------------------------

/// <summary>
/// SKILL.md YAML 프론트매터의 역직렬화 대상 클래스입니다.
/// YamlDotNet의 HyphenatedNamingConvention으로 kebab-case 키를 자동 매핑합니다.
/// </summary>
internal sealed class SkillFrontmatter
{
    public string? Name { get; set; }
    public string? Description { get; set; }
    public bool DisableModelInvocation { get; set; }
    public bool UserInvocable { get; set; } = true;
}