using Microsoft.AspNetCore.Components;
using Microsoft.JSInterop;
using UnrealAgent.Frontend.Infrastructure;

namespace UnrealAgent.Frontend.UI.Input;

public partial class ImagePicker : JsComponentBase
{
    /// <summary>이미지 변경 시 부모에 알리는 콜백입니다.</summary>
    [Parameter] public EventCallback OnChanged { get; set; }

    /// <summary>현재 첨부된 이미지의 MIME 타입입니다.</summary>
    public string? ImageMediaType { get; private set; }

    /// <summary>현재 첨부된 이미지의 Base64 데이터입니다.</summary>
    public string? ImageBase64 { get; private set; }

    /// <summary>파일 다이얼로그 참조입니다.</summary>
    private ElementReference FileInputRef;

    /// <summary>.NET에서 JS가 호출할 수 있는 참조입니다.</summary>
    private DotNetObjectReference<ImagePicker>? DotNetRef;

    protected override Task OnModuleLoaded()
    {
        DotNetRef = DotNetObjectReference.Create(this);
        return Task.CompletedTask;
    }

    /// <summary>파일 다이얼로그를 엽니다.</summary>
    private async Task Pick()
    {
        await Module!.InvokeVoidAsync("pick", FileInputRef, DotNetRef);
    }

    /// <summary>JS에서 이미지 선택 완료 시 호출됩니다.</summary>
    [JSInvokable]
    public async Task OnImagePicked(string MediaType, string Base64)
    {
        ImageMediaType = MediaType;
        ImageBase64 = Base64;

        await OnChanged.InvokeAsync();
    }

    /// <summary>첨부된 이미지를 제거합니다.</summary>
    public async Task Clear()
    {
        ImageMediaType = null;
        ImageBase64 = null;
        
        await OnChanged.InvokeAsync();
    }
}