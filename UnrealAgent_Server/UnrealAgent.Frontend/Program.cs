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

MessageCreateParams Parameters = new MessageCreateParams
{
    Model = "claude-opus-4-6",
    MaxTokens = 1024,
    Messages = [new() { Role = Role.User, Content = "안녕하세요! 간단히 자기소개 해주세요. " }]
};

Message Response = await Auth.Client!.Messages.Create(Parameters);

foreach (ContentBlock Block in Response.Content)
{
    if (Block.TryPickText(out var Text))
        Console.WriteLine(Text.Text);
}