namespace UnrealAgent.Backend.Conversation;

/// <summary>
/// 사용자 입력 메시지입니다. 텍스트와 첨부 이미지를 포함합니다.
/// </summary>
public sealed record UserInput(string Text, string? ImageMediaType = null, string? ImageBase64 = null)
{
    public static implicit operator UserInput(string Text) => new(Text);
    
    /// <summary>첨부 이미지가 있는지 여부입니다.</summary>
    public bool bHasImage => ImageBase64 is not null;
}