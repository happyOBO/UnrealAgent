using UnrealAgent.Backend.Core;

namespace UnrealAgent.Backend.Mention;

/// <summary>
/// 프로젝트 파일/폴더 목록을 제공하는 멘션 데이터 소스입니다.
/// </summary>
public static class MentionProvider
{
    /// <summary>목록에서 제외할 폴더 이름 집합입니다.</summary>
    private static readonly HashSet<string> ExcludedFolders = new(StringComparer.OrdinalIgnoreCase)
    {
        "Binaries", "Intermediate", "DerivedDataCache", "Saved",
        "obj", "bin", "Build", "node_modules", "Collections", "Developers"
    };
    
    /// <summary>목록에서 제외할 파일 확장자 집합입니다.</summary>
    private static readonly HashSet<string> ExcludedExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".md", ".sln", ".slnx", ".uproject", ".cs", ".uplugin"
    };

    /// <summary>
    /// 지정 경로의 하위 폴더와 파일 목록을 반환합니다.
    /// </summary>
    /// <param name="BasePath">프로젝트 루트 기준 상대 경로입니다. 빈 문자열이면 루트입니다.</param>
    /// <param name="Filter">이름 필터입니다. 빈 문자열이면 전체를 반환합니다.</param>
    public static List<MentionItem> ListItems(string BasePath, string Filter)
    {
        string FullPath = string.IsNullOrEmpty(BasePath)
            ? AgentPaths.RootPath
            : Path.Combine(AgentPaths.RootPath, BasePath);

        if (!Directory.Exists(FullPath))
            return [];

        List<MentionItem> Items = [];

        // 폴더 목록을 수집합니다.
        foreach (string Dir in Directory.EnumerateDirectories(FullPath))
        {
            string Name = Path.GetFileName(Dir);
            
            // .git, .vs, .idea, .vscode 등 .으로 시작하면 무시
            if (Name.StartsWith('.'))
                continue;
            
            // 제외할 폴더면 무시
            if (ExcludedFolders.Contains(Name))
                continue;
            
            // Filter에 포함되지 않으면 무시
            if (!string.IsNullOrEmpty(Filter) && !Name.Contains(Filter, StringComparison.OrdinalIgnoreCase))
                continue;
            
            // 폴더 아이템 추가
            string RelativePath = string.IsNullOrEmpty(BasePath) ? Name : $"{BasePath}/{Name}";
            Items.Add(new MentionItem(Name, RelativePath, MentionItemKind.Folder));
        }
        
        // 파일 목록을 수집합니다.
        foreach (string FilePath in Directory.EnumerateFiles(FullPath))
        {
            string Name = Path.GetFileName(FilePath);
            
            if (Name.StartsWith('.'))
                continue;
            
            // 제외할 확장자면 무시
            string Extension = Path.GetExtension(Name);
            if (ExcludedExtensions.Contains(Extension))
                continue;
            
            if (!string.IsNullOrEmpty(Filter) && !Name.Contains(Filter, StringComparison.OrdinalIgnoreCase))
                continue;
            
            // 파일 아이템 추가
            string RelativePath = string.IsNullOrEmpty(BasePath) ? Name : $"{BasePath}/{Name}";
            Items.Add(new MentionItem(Name, RelativePath, MentionItemKind.File));
        }
        
        // 폴더 우선, 이름순으로 정렬합니다.
        Items.Sort((A, B) =>
        {
            int KindCompare = A.Kind.CompareTo(B.Kind);
            return KindCompare != 0
                ? KindCompare
                : string.Compare(A.Name, B.Name, StringComparison.OrdinalIgnoreCase);
        });
        
        return Items;
    }

    /// <summary>
    /// 프로젝트 전체에서 이름이 일치하는 파일과 폴더를 재귀적으로 검색합니다.
    /// </summary>
    /// <param name="Filter">검색 키워드입니다.</param>
    /// <param name="MaxResults">최대 결과 수입니다.</param>
    public static List<MentionItem> SearchItems(string Filter, int MaxResults = 30)
    {
        List<MentionItem> Items = [];
        SearchRecursive(AgentPaths.RootPath, "", Filter, Items, MaxResults);
        
        Items.Sort((A, B) =>
        {
            int KindCompare = A.Kind.CompareTo(B.Kind);
            return KindCompare != 0
                ? KindCompare
                : string.Compare(A.Name, B.Name, StringComparison.OrdinalIgnoreCase);
        });

        return Items;
    }

    /// <summary>
    /// 디렉토리를 재귀적으로 탐색하며 이름이 일치하는 항목을 수집합니다.
    /// </summary>
    private static void SearchRecursive(string FullPath, string BasePath, string Filter, List<MentionItem> Items, int MaxResults)
    {
        if (Items.Count >= MaxResults)
            return;
        
        // 현재 경로의 폴더를 탐색합니다.
        foreach (string Dir in Directory.EnumerateDirectories(FullPath))
        {
            string Name = Path.GetFileName(Dir);
            
            if (ExcludedFolders.Contains(Name) || Name.StartsWith('.'))
                continue;
            
            // Filter에 포함되어 있는 폴더면 등록
            string RelativePath = string.IsNullOrEmpty(BasePath) ? Name : $"{BasePath}/{Name}";
            if (Name.Contains(Filter, StringComparison.OrdinalIgnoreCase))
            {
                Items.Add(new MentionItem(Name, RelativePath, MentionItemKind.Folder));
                if (Items.Count >= MaxResults)
                    return;
            }
            
            // 하위 폴더를 재귀 탐색합니다.
            SearchRecursive(Dir, RelativePath, Filter, Items, MaxResults);
            if (Items.Count >= MaxResults)
                return;
        }
        
        // 현재 경로의 파일을 탐색합니다.
        foreach (string FilePath in Directory.EnumerateFiles(FullPath))
        {
            string Name = Path.GetFileName(FilePath);

            if (Name.StartsWith('.'))
                continue;
            
            if (ExcludedExtensions.Contains(Path.GetExtension(Name)))
                continue;
            
            if (!Name.Contains(Filter, StringComparison.OrdinalIgnoreCase))
                continue;
            
            string RelativePath = string.IsNullOrEmpty(BasePath) ? Name : $"{BasePath}/{Name}";
            
            // Filter에 포함되어 있는 파일이면 등록
            Items.Add(new MentionItem(Name, RelativePath, MentionItemKind.File));
            if (Items.Count >= MaxResults)
                return;
        }
    }
}