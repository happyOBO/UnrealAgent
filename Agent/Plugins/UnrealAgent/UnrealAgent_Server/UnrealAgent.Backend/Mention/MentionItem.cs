namespace UnrealAgent.Backend.Mention;

//-----------------------------------------------------------------------------
// MentionItemKind
//-----------------------------------------------------------------------------

/// <summary>
/// 멘션 항목의 종류입니다.
/// </summary>
public enum MentionItemKind
{
    Folder,
    File
}

//-----------------------------------------------------------------------------
// MentionItem
//-----------------------------------------------------------------------------

/// <summary>
/// @ 멘션 팝업에 표시되는 개별 항목입니다.
/// </summary>
/// <param name="Name">파일 또는 폴더 이름입니다.</param>
/// <param name="RelativePath">프로젝트 루트 기준 상대 경로입니다.</param>
/// <param name="Kind">폴더 또는 파일 구분입니다.</param>
public record MentionItem(string Name, string RelativePath, MentionItemKind Kind);