#include "Tools/GetLevelActorsTool.h"
#include "Tools/ActorToolHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(GetLevelActorsTool)

namespace
{
	constexpr int32 AbsoluteMaxActors = 500;
}

FMcpResponse FGetLevelActorsTool::Execute()
{
	UWorld* World = ActorToolHelpers::GetEditorWorld();
	if (!World)
	{
		return FMcpResponse::Failure(TEXT("No editor world available. Open a level first."));
	}

	const int32 Cap = FMath::Clamp(Limit <= 0 ? 50 : Limit, 1, AbsoluteMaxActors);
	const int32 Skip = FMath::Max(0, Offset);

	TArray<FString> Lines;
	int32 TotalMatching = 0;
	int32 Added = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		if (!ClassFilter.IsEmpty() &&
			!Actor->GetClass()->GetName().Contains(ClassFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (!NameFilter.IsEmpty() &&
			!Actor->GetName().Contains(NameFilter, ESearchCase::IgnoreCase) &&
			!Actor->GetActorLabel().Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		const int32 MatchIndex = TotalMatching;
		TotalMatching++;

		if (MatchIndex < Skip || Added >= Cap)
		{
			continue;
		}

		const FVector Loc = Actor->GetActorLocation();
		Lines.Add(FString::Printf(TEXT("- %s [label: %s] (%s) @ (%.0f, %.0f, %.0f)"),
			*Actor->GetName(), *Actor->GetActorLabel(), *Actor->GetClass()->GetName(),
			Loc.X, Loc.Y, Loc.Z));
		Added++;
	}

	const bool bHasMore = (Skip + Added) < TotalMatching;

	FString Header = FString::Printf(TEXT("Level '%s': %d actor(s) matched"), *World->GetMapName(), TotalMatching);
	if (TotalMatching > Added)
	{
		Header += FString::Printf(TEXT(", showing %d-%d"), Skip + 1, Skip + Added);
	}
	if (bHasMore)
	{
		Header += FString::Printf(TEXT(" (more available — use offset=%d for next page)"), Skip + Added);
	}
	Header += TEXT(":\n");

	if (Added == 0)
	{
		return FMcpResponse::Success(Header + TEXT("(no actors matched the filters)"));
	}

	return FMcpResponse::Success(Header + FString::Join(Lines, TEXT("\n")));
}
