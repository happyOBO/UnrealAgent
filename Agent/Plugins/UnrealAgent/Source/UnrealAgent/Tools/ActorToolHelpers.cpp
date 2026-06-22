#include "Tools/ActorToolHelpers.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"

namespace ActorToolHelpers
{
	UWorld* GetEditorWorld()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		return GEditor->GetEditorWorldContext().World();
	}

	AActor* FindActorByNameOrLabel(UWorld* World, const FString& NameOrLabel)
	{
		if (!World || NameOrLabel.IsEmpty())
		{
			return nullptr;
		}

		AActor* PartialMatch = nullptr;

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor)
			{
				continue;
			}

			const FString Name = Actor->GetName();
			const FString Label = Actor->GetActorLabel();

			// 정확 일치 우선.
			if (Name.Equals(NameOrLabel, ESearchCase::IgnoreCase) ||
				Label.Equals(NameOrLabel, ESearchCase::IgnoreCase))
			{
				return Actor;
			}

			// 부분 일치는 폴백으로 보관.
			if (!PartialMatch &&
				(Name.Contains(NameOrLabel, ESearchCase::IgnoreCase) ||
				 Label.Contains(NameOrLabel, ESearchCase::IgnoreCase)))
			{
				PartialMatch = Actor;
			}
		}

		return PartialMatch;
	}

	UClass* ResolveActorClass(const FString& ClassPathOrName, FString& OutError)
	{
		OutError.Reset();

		if (ClassPathOrName.IsEmpty())
		{
			OutError = TEXT("class_path is empty.");
			return nullptr;
		}

		// 경로 형태(/Game/... 또는 /Script/...)면 오브젝트로 로드.
		if (ClassPathOrName.StartsWith(TEXT("/")))
		{
			if (UObject* Loaded = LoadObject<UObject>(nullptr, *ClassPathOrName))
			{
				if (UBlueprint* Blueprint = Cast<UBlueprint>(Loaded))
				{
					if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
					{
						return Blueprint->GeneratedClass;
					}
					OutError = FString::Printf(TEXT("Blueprint '%s' is not an Actor class."), *ClassPathOrName);
					return nullptr;
				}
				if (UClass* AsClass = Cast<UClass>(Loaded))
				{
					if (AsClass->IsChildOf(AActor::StaticClass()))
					{
						return AsClass;
					}
					OutError = FString::Printf(TEXT("Class '%s' is not an Actor class."), *ClassPathOrName);
					return nullptr;
				}
			}

			// _C 접미사가 없는 블루프린트 경로를 명시적으로 재시도.
			if (UClass* GenClass = LoadObject<UClass>(nullptr, *(ClassPathOrName + TEXT("_C"))))
			{
				if (GenClass->IsChildOf(AActor::StaticClass()))
				{
					return GenClass;
				}
			}

			OutError = FString::Printf(TEXT("Could not load an Actor class from path '%s'."), *ClassPathOrName);
			return nullptr;
		}

		// 짧은 이름: 엔진/게임 모듈에서 UClass 검색 (예: StaticMeshActor, PointLight).
		FString CleanName = ClassPathOrName;
		CleanName.RemoveFromStart(TEXT("A"));   // 사용자가 'AStaticMeshActor'로 줄 수도 있으나 보통 'StaticMeshActor'

		// 우선 정확 이름으로 클래스 검색.
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class->IsChildOf(AActor::StaticClass()))
			{
				continue;
			}
			if (Class->GetName().Equals(ClassPathOrName, ESearchCase::IgnoreCase) ||
				Class->GetName().Equals(CleanName, ESearchCase::IgnoreCase))
			{
				return Class;
			}
		}

		OutError = FString::Printf(
			TEXT("Could not resolve Actor class '%s'. Use an engine class short name (e.g. 'StaticMeshActor', 'PointLight') or a Blueprint path (e.g. '/Game/BP_Door')."),
			*ClassPathOrName);
		return nullptr;
	}
}
