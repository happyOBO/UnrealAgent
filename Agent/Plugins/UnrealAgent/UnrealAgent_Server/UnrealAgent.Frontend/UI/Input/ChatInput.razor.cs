using Microsoft.AspNetCore.Components;
using Microsoft.JSInterop;
using UnrealAgent.Backend.Conversation;
using UnrealAgent.Frontend.Infrastructure;

namespace UnrealAgent.Frontend.UI.Input;

public partial class ChatInput : JsComponentBase
{
    /// <summary>메시지 전송 콜백입니다.</summary>
    [Parameter] public EventCallback<UserInput> OnSend { get; set; }

    /// <summary>현재 입력 텍스트입니다.</summary>
    private string InputText = "";

    /// <summary>textarea 요소 참조입니다.</summary>
    private ElementReference TextAreaRef;

    /// <summary>JS 모듈 로드 후 Enter 키 바인딩을 설정합니다.</summary>
    protected override async Task OnModuleLoaded()
    {
        await Module.InvokeVoidAsync("setupEnterSubmit", TextAreaRef);
    }

    /// <summary>폼 제출 시 메시지를 전송합니다.</summary>
    private async Task HandleSubmit()
    {
        string Trimmed = InputText.Trim();

        if (string.IsNullOrEmpty(Trimmed))
            return;

        InputText = "";
        await OnSend.InvokeAsync(Trimmed);
    }
}