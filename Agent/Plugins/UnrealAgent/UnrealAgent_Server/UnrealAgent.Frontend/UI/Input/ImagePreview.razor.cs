using Microsoft.AspNetCore.Components;

namespace UnrealAgent.Frontend.UI.Input;

public partial class ImagePreview : ComponentBase
{
    /// <summary>이미지의 MIME 타입입니다. null이면 미리보기를 표시하지 않습니다.</summary>
    [Parameter] public string? ImageMediaType { get; set; }

    /// <summary>이미지의 Base64 데이터입니다.</summary>
    [Parameter] public string? ImageBase64 { get; set; }

    /// <summary>제거 버튼 클릭 시 콜백입니다.</summary>
    [Parameter] public EventCallback OnRemove { get; set; }
}