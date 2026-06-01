using Microsoft.AspNetCore.Components;
using UnrealAgent.Backend.Chat;

namespace UnrealAgent.Frontend.UI.Messages;

public partial class ChatMessages
{
    /// <summary>표시할 메시지 목록입니다.</summary>
    [Parameter] public List<ChatUIMessage> Messages { get; set; } = [];
}