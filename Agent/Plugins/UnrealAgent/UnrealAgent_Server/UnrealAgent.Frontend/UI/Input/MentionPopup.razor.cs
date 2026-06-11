using Microsoft.JSInterop;
using UnrealAgent.Backend.Mention;
using UnrealAgent.Frontend.Infrastructure;

namespace UnrealAgent.Frontend.UI.Input;

public partial class MentionPopup : JsComponentBase
{
    /// <summary>팝업 표시 여부입니다.</summary>
    public bool bShowPopup { get; private set; }

    /// <summary>현재 선택된 인덱스입니다.</summary>
    private int SelectedIndex;

    /// <summary>멘션 항목 목록입니다.</summary>
    private List<MentionItem> Items = [];

    /// <summary>현재 탐색 중인 경로입니다. 빈 문자열이면 프로젝트 루트입니다.</summary>
    private string BasePath = "";

    /// <summary>
    /// 전체 입력 텍스트로 팝업을 갱신합니다.
    /// 마지막 @를 찾아 멘션 쿼리를 추출합니다.
    /// </summary>
    public void Update(string Text)
    {
        // 마지막 @를 찾습니다.
        int AtIndex = Text.LastIndexOf('@');

        if (AtIndex < 0)
        {
            if (bShowPopup)
                Close();

            return;
        }

        string Query = Text[(AtIndex + 1)..];

        // 쿼리에 공백이 있으면 완료된 멘션입니다.
        if (Query.Contains(' '))
        {
            if (bShowPopup) 
                Close();
            
            return;
        }

        // @ 뒤 쿼리로 항목을 갱신합니다.
        int LastSlash = Query.LastIndexOf('/');
        
        // @Source/My (Source 폴더 안에서 My 필터)
        if (LastSlash >= 0)
        {
            BasePath = Query[..LastSlash];
            string Filter = Query[(LastSlash + 1)..];
            Items = MentionProvider.ListItems(BasePath, Filter);
        }
        // @So (프로젝트 전체 재귀 검색)
        else if (Query.Length >= 2)
        {
            BasePath = "";
            Items = MentionProvider.SearchItems(Query);
        }
        // @ (루트 폴더 목록 표시)
        else
        {
            BasePath = "";
            Items = MentionProvider.ListItems("", Query);
        }

        bShowPopup = Items.Count > 0 || !string.IsNullOrEmpty(BasePath);
        SelectedIndex = 0;
        StateHasChanged();
    }

    /// <summary>방향키로 항목을 탐색합니다.</summary>
    public async Task Navigate(int Direction)
    {
        if (!bShowPopup || Items.Count == 0)
            return;

        SelectedIndex = (SelectedIndex + Direction + Items.Count) % Items.Count;
        await Module!.InvokeVoidAsync("scrollToItem", "mention-item", SelectedIndex);
        
        StateHasChanged();
    }

    /// <summary>
    /// Enter 키로 최종 선택합니다.
    /// 폴더든 파일이든 경로를 반환하고 팝업을 닫습니다.
    /// </summary>
    public string? Select()
    {
        if (!bShowPopup || SelectedIndex < 0 || SelectedIndex >= Items.Count)
            return null;

        string Path = Items[SelectedIndex].RelativePath;
        Close();
        
        return Path;
    }

    /// <summary>
    /// Tab 키로 폴더면 드릴다운, 파일이면 선택합니다.
    /// 드릴다운 시 "경로/"를 반환합니다.
    /// </summary>
    public string? Tab()
    {
        if (!bShowPopup || SelectedIndex < 0 || SelectedIndex >= Items.Count)
            return null;

        MentionItem Selected = Items[SelectedIndex];

        if (Selected.Kind == MentionItemKind.Folder)
            return Selected.RelativePath + "/";

        Close();
        
        return Selected.RelativePath;
    }

    /// <summary>
    /// ← 키로 상위 폴더 쿼리를 반환합니다.
    /// BasePath가 없으면 null을 반환합니다.
    /// </summary>
    public string? GoBack()
    {
        if (string.IsNullOrEmpty(BasePath))
            return null;

        int LastSlash = BasePath.LastIndexOf('/');
        return LastSlash >= 0 ? BasePath[..LastSlash] + "/" : "";
    }

    /// <summary>팝업을 닫고 상태를 초기화합니다.</summary>
    public void Close()
    {
        bShowPopup = false;
        BasePath = "";
        Items = [];
        SelectedIndex = 0;
        
        StateHasChanged();
    }
}