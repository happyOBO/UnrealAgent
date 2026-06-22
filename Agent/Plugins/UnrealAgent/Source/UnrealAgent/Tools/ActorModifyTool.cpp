#include "Tools/ActorModifyTool.h"
#include "Tools/ActorToolHelpers.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorActorSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorModifyTool)

#define LOCTEXT_NAMESPACE "ActorModifyTool"

namespace
{
	// 액터에서 property를 찾습니다. "Component.Property" 형태면 컴포넌트로 1단계 내려갑니다.
	// 성공 시 대상 UObject와 FProperty를 반환합니다.
	bool ResolveProperty(AActor* Actor, const FString& PropertyPath, UObject*& OutObject, FProperty*& OutProperty, FString& OutError)
	{
		OutObject = Actor;
		OutProperty = nullptr;

		TArray<FString> Parts;
		PropertyPath.ParseIntoArray(Parts, TEXT("."), true);
		if (Parts.Num() == 0)
		{
			OutError = TEXT("property is empty.");
			return false;
		}

		// 컴포넌트 프리픽스 처리 (마지막 세그먼트만 실제 프로퍼티).
		if (Parts.Num() >= 2)
		{
			const FString& CompName = Parts[0];
			UActorComponent* Found = nullptr;
			for (UActorComponent* Comp : Actor->GetComponents())
			{
				if (Comp && Comp->GetName().Contains(CompName, ESearchCase::IgnoreCase))
				{
					Found = Comp;
					break;
				}
			}

			if (!Found)
			{
				OutError = FString::Printf(TEXT("Component '%s' not found on actor '%s'."), *CompName, *Actor->GetName());
				return false;
			}

			OutObject = Found;
			OutProperty = Found->GetClass()->FindPropertyByName(FName(*Parts.Last()));
		}
		else
		{
			OutProperty = Actor->GetClass()->FindPropertyByName(FName(*Parts[0]));
		}

		if (!OutProperty)
		{
			OutError = FString::Printf(TEXT("Property '%s' not found on %s."),
				*Parts.Last(), *OutObject->GetClass()->GetName());
			return false;
		}

		return true;
	}
}

FMcpResponse FActorModifyTool::Execute()
{
	if (!GEditor)
	{
		return FMcpResponse::Failure(TEXT("Editor is not available."));
	}

	const FString Op = Operation.TrimStartAndEnd().ToLower();

	if (Op == TEXT("spawn"))        return ExecuteSpawn();
	if (Op == TEXT("move"))         return ExecuteMove();
	if (Op == TEXT("delete"))       return ExecuteDelete();
	if (Op == TEXT("set_property")) return ExecuteSetProperty();

	return FMcpResponse::Failure(FString::Printf(
		TEXT("Unknown operation '%s'. Use: spawn, move, delete, set_property."), *Operation));
}

FMcpResponse FActorModifyTool::ExecuteSpawn()
{
	UWorld* World = ActorToolHelpers::GetEditorWorld();
	if (!World)
	{
		return FMcpResponse::Failure(TEXT("No editor world available. Open a level first."));
	}

	FString Error;
	UClass* ActorClass = ActorToolHelpers::ResolveActorClass(ClassPath, Error);
	if (!ActorClass)
	{
		return FMcpResponse::Failure(Error);
	}

	const FScopedTransaction Transaction(LOCTEXT("SpawnActor", "Spawn Actor (Agent)"));

	const FVector Location(LocX, LocY, LocZ);
	const FRotator Rotation(RotPitch, RotYaw, RotRoll);
	const FVector Scale(ScaleX, ScaleY, ScaleZ);
	const FTransform SpawnTransform(Rotation, Location, Scale);

	FActorSpawnParameters SpawnParams;
	if (!NewName.IsEmpty())
	{
		SpawnParams.Name = FName(*NewName);
	}
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
	if (!Spawned)
	{
		return FMcpResponse::Failure(FString::Printf(TEXT("Failed to spawn actor of class '%s'."), *ActorClass->GetName()));
	}

	if (!NewName.IsEmpty())
	{
		Spawned->SetActorLabel(NewName);
	}
	Spawned->MarkPackageDirty();

	return FMcpResponse::Success(FString::Printf(
		TEXT("Spawned '%s' (label '%s', class %s) at (%.0f, %.0f, %.0f)."),
		*Spawned->GetName(), *Spawned->GetActorLabel(), *ActorClass->GetName(), LocX, LocY, LocZ));
}

FMcpResponse FActorModifyTool::ExecuteMove()
{
	UWorld* World = ActorToolHelpers::GetEditorWorld();
	if (!World)
	{
		return FMcpResponse::Failure(TEXT("No editor world available. Open a level first."));
	}

	AActor* Actor = ActorToolHelpers::FindActorByNameOrLabel(World, ActorName);
	if (!Actor)
	{
		return FMcpResponse::Failure(FString::Printf(TEXT("Actor '%s' not found."), *ActorName));
	}

	if (!bApplyLocation && !bApplyRotation && !bApplyScale)
	{
		return FMcpResponse::Failure(TEXT("No transform to apply. Set apply_location, apply_rotation, and/or apply_scale to true."));
	}

	const FScopedTransaction Transaction(LOCTEXT("MoveActor", "Move Actor (Agent)"));
	Actor->Modify();

	TArray<FString> Changes;

	if (bApplyLocation)
	{
		const FVector New = bRelative ? Actor->GetActorLocation() + FVector(LocX, LocY, LocZ) : FVector(LocX, LocY, LocZ);
		Actor->SetActorLocation(New);
		Changes.Add(TEXT("location"));
	}

	if (bApplyRotation)
	{
		const FRotator Delta(RotPitch, RotYaw, RotRoll);
		const FRotator New = bRelative ? Actor->GetActorRotation() + Delta : Delta;
		Actor->SetActorRotation(New);
		Changes.Add(TEXT("rotation"));
	}

	if (bApplyScale)
	{
		const FVector S(ScaleX, ScaleY, ScaleZ);
		const FVector New = bRelative ? Actor->GetActorScale3D() * S : S;
		Actor->SetActorScale3D(New);
		Changes.Add(TEXT("scale"));
	}

	Actor->MarkPackageDirty();

	const FVector L = Actor->GetActorLocation();
	return FMcpResponse::Success(FString::Printf(
		TEXT("Updated %s on '%s'. Now at (%.0f, %.0f, %.0f)."),
		*FString::Join(Changes, TEXT(", ")), *Actor->GetName(), L.X, L.Y, L.Z));
}

FMcpResponse FActorModifyTool::ExecuteDelete()
{
	UWorld* World = ActorToolHelpers::GetEditorWorld();
	if (!World)
	{
		return FMcpResponse::Failure(TEXT("No editor world available. Open a level first."));
	}

	AActor* Actor = ActorToolHelpers::FindActorByNameOrLabel(World, ActorName);
	if (!Actor)
	{
		return FMcpResponse::Failure(FString::Printf(TEXT("Actor '%s' not found."), *ActorName));
	}

	const FString Name = Actor->GetName();
	const FString Label = Actor->GetActorLabel();

	const FScopedTransaction Transaction(LOCTEXT("DeleteActor", "Delete Actor (Agent)"));

	if (UEditorActorSubsystem* ActorSub = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
	{
		ActorSub->DestroyActor(Actor);
	}
	else
	{
		World->EditorDestroyActor(Actor, true);
	}

	return FMcpResponse::Success(FString::Printf(TEXT("Deleted actor '%s' (label '%s')."), *Name, *Label));
}

FMcpResponse FActorModifyTool::ExecuteSetProperty()
{
	UWorld* World = ActorToolHelpers::GetEditorWorld();
	if (!World)
	{
		return FMcpResponse::Failure(TEXT("No editor world available. Open a level first."));
	}

	AActor* Actor = ActorToolHelpers::FindActorByNameOrLabel(World, ActorName);
	if (!Actor)
	{
		return FMcpResponse::Failure(FString::Printf(TEXT("Actor '%s' not found."), *ActorName));
	}

	if (Property.IsEmpty())
	{
		return FMcpResponse::Failure(TEXT("property is required for set_property."));
	}

	UObject* TargetObject = nullptr;
	FProperty* TargetProperty = nullptr;
	FString Error;
	if (!ResolveProperty(Actor, Property, TargetObject, TargetProperty, Error))
	{
		return FMcpResponse::Failure(Error);
	}

	const FScopedTransaction Transaction(LOCTEXT("SetActorProperty", "Set Actor Property (Agent)"));
	TargetObject->Modify();

	// UE의 ImportText는 숫자/불/문자열/이름/열거형/구조체("(X=1,Y=2,Z=3)")/오브젝트 참조 경로를
	// 모두 처리합니다. 단일 진입점으로 대부분의 타입을 안전하게 커버합니다.
	void* ValuePtr = TargetProperty->ContainerPtrToValuePtr<void>(TargetObject);
	const TCHAR* ImportResult = TargetProperty->ImportText_Direct(*Value, ValuePtr, TargetObject, PPF_None);

	if (ImportResult == nullptr)
	{
		return FMcpResponse::Failure(FString::Printf(
			TEXT("Could not set '%s' (type %s) from value '%s'. Use a number/bool/string, UE struct text '(X=1,Y=2,Z=3)', or an asset path for object properties."),
			*Property, *TargetProperty->GetCPPType(), *Value));
	}

	// 스트리밍/물리/내비 시스템에 변경을 통지합니다. 오브젝트 참조(예: StaticMesh) 설정 시
	// 이 호출이 없으면 스트리밍 매니저가 불일치를 뒤늦게 감지해 크래시할 수 있습니다.
	FPropertyChangedEvent ChangeEvent(TargetProperty, EPropertyChangeType::ValueSet);
	TargetObject->PostEditChangeProperty(ChangeEvent);
	Actor->MarkPackageDirty();

	return FMcpResponse::Success(FString::Printf(
		TEXT("Set '%s' = '%s' on '%s'."), *Property, *Value, *Actor->GetName()));
}

#undef LOCTEXT_NAMESPACE
