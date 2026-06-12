namespace UnrealAgent.Backend.Conversation;

/// <summary>
/// 사용자 입력 메시지입니다. 텍스트와 첨부 이미지를 포함합니다.
/// </summary>
public sealed record UserInput(string Text, string? ImageMediaType = null, string? ImageBase64 = null)
{
    public static implicit operator UserInput(string Text) => new(Text);

    /// <summary>첨부 이미지가 있는지 여부입니다.</summary>
    public bool bHasImage => ImageBase64 is not null;

    /// <summary>
    /// claude CLI 내장 슬래시 커맨드(/compact 등) 호출 여부입니다.
    /// true면 해당 턴에서 CLI의 --disable-slash-commands 격리를 해제합니다.
    /// </summary>
    public bool bCliCommand { get; init; }
}