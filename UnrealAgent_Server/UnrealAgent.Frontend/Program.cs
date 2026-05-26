using Anthropic.Models.Messages;
using Microsoft.Extensions.DependencyInjection;
using UnrealAgent.Backend.Auth;

ServiceCollection Services = new ServiceCollection();
Services.AddSingleton<AuthConfig>();

ServiceProvider Provider = Services.BuildServiceProvider();
AuthConfig Auth = Provider.GetRequiredService<AuthConfig>();

Auth.Load();

if (!Auth.IsApiKeyConfigured())
{
    Console.Write("API Key를 입력하세요: ");
    string? Key = Console.ReadLine();

    if (string.IsNullOrWhiteSpace(Key))
    {
        Console.WriteLine("API Key가 입력되지 않았습니다.");
        return;
    }

    Auth.SetApiKey(Key);
    Console.WriteLine("API Key 저장 완료!");
}

while (true)
{
    Console.Write("\n> ");
    string? Input = Console.ReadLine();

    if (string.IsNullOrWhiteSpace(Input)) 
        continue;
    
    if (Input.Equals("exit", StringComparison.OrdinalIgnoreCase)) 
        break;

    var Parameters = new MessageCreateParams
    {
        Model = "claude-opus-4-6",
        MaxTokens = 1024,
        Messages = [new() { Role = Role.User, Content = Input }]
    };

    await foreach (RawMessageStreamEvent Event in Auth.Client!.Messages.CreateStreaming(Parameters))
    {
        if (Event.TryPickContentBlockDelta(out RawContentBlockDeltaEvent? delta) && delta.Delta.TryPickText(out TextDelta? text))
        {
            Console.Write(text.Text);
        }
    }
    
    Console.WriteLine();
}
