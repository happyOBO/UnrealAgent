#include "Chat/AgentChatInputProcessor.h"
#include "Framework/Commands/UICommandList.h"

FAgentChatInputProcessor::FAgentChatInputProcessor(const TSharedPtr<FUICommandList>& InCommands)
	: Commands(InCommands)
{
}

void FAgentChatInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	// IInputProcessor의 순수 가상 함수라 구현이 필요합니다. 매 틱 처리할 작업은 없습니다.
}

bool FAgentChatInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent)
{
	if (const TSharedPtr<FUICommandList> PinnedCommands = Commands.Pin())
	{
		return PinnedCommands->ProcessCommandBindings(KeyEvent);
	}

	return false;
}
