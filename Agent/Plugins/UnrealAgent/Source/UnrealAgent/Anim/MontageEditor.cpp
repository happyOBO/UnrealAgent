#include "Anim/MontageEditor.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Factories/AnimMontageFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

//-----------------------------------------------------------------------------
// 로드 / 조회
//-----------------------------------------------------------------------------

UAnimMontage* FMontageEditor::LoadMontage(const FString& MontagePath, FString& OutError)
{
	UObject* Loaded = StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *MontagePath);
	if (!Loaded && !MontagePath.StartsWith(TEXT("/")))
		Loaded = StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *(TEXT("/Game/") + MontagePath));

	UAnimMontage* Montage = Cast<UAnimMontage>(Loaded);
	if (!Montage)
	{
		OutError = FString::Printf(TEXT("Montage not found: %s"), *MontagePath);
		return nullptr;
	}

	const UPackage* Package = Montage->GetPackage();
	if (Package && Package->HasAnyPackageFlags(PKG_Cooked))
	{
		OutError = TEXT("Cooked Montage cannot be modified.");
		return nullptr;
	}
	return Montage;
}

static int32 FindSlotIndex(UAnimMontage* Montage, const FString& SlotName)
{
	for (int32 i = 0; i < Montage->SlotAnimTracks.Num(); ++i)
	{
		if (Montage->SlotAnimTracks[i].SlotName.ToString().Equals(SlotName, ESearchCase::IgnoreCase))
			return i;
	}
	return INDEX_NONE;
}

//-----------------------------------------------------------------------------
// 생성
//-----------------------------------------------------------------------------

UAnimMontage* FMontageEditor::CreateMontage(const FString& MontagePath, const FString& SkeletonPath, const FString& AnimSequencePath, FString& OutError)
{
	// 스켈레톤 결정: AnimSequence의 스켈레톤 또는 명시된 SkeletonPath
	UAnimSequence* Sequence = nullptr;
	if (!AnimSequencePath.IsEmpty())
	{
		Sequence = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *AnimSequencePath));
		if (!Sequence) { OutError = FString::Printf(TEXT("AnimSequence not found: %s"), *AnimSequencePath); return nullptr; }
	}

	USkeleton* Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	if (!SkeletonPath.IsEmpty())
	{
		USkeleton* Loaded = Cast<USkeleton>(StaticLoadObject(USkeleton::StaticClass(), nullptr, *SkeletonPath));
		if (!Loaded) { OutError = FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath); return nullptr; }
		Skeleton = Loaded;
	}
	if (!Skeleton)
	{
		OutError = TEXT("create requires 'skeleton' or 'anim_sequence' to determine the target skeleton.");
		return nullptr;
	}

	// 패키지/에셋 이름 분해
	FString PackageName = MontagePath;
	if (!PackageName.StartsWith(TEXT("/")))
		PackageName = TEXT("/Game/") + PackageName;
	const FString AssetName = FPackageName::GetShortName(PackageName);

	if (StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *PackageName))
	{
		OutError = FString::Printf(TEXT("Montage already exists: %s"), *PackageName);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package) { OutError = TEXT("Failed to create package"); return nullptr; }

	UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
	Factory->TargetSkeleton = Skeleton;
	Factory->SourceAnimation = Sequence; // null이면 빈 몽타주

	UAnimMontage* Montage = Cast<UAnimMontage>(Factory->FactoryCreateNew(
		UAnimMontage::StaticClass(), Package, FName(*AssetName),
		RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
	if (!Montage)
	{
		OutError = TEXT("Factory failed to create montage");
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(Montage);
	Montage->MarkPackageDirty();
	return Montage;
}

//-----------------------------------------------------------------------------
// 슬롯 편집
//-----------------------------------------------------------------------------

bool FMontageEditor::RenameSlot(UAnimMontage* Montage, const FString& OldSlotName, const FString& NewSlotName, FString& OutError)
{
	if (!Montage) { OutError = TEXT("Null montage"); return false; }
	if (NewSlotName.IsEmpty()) { OutError = TEXT("new_slot_name is empty"); return false; }

	const int32 Index = FindSlotIndex(Montage, OldSlotName);
	if (Index == INDEX_NONE) { OutError = FString::Printf(TEXT("Slot '%s' not found"), *OldSlotName); return false; }

	if (FindSlotIndex(Montage, NewSlotName) != INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("Slot '%s' already exists"), *NewSlotName);
		return false;
	}

	Montage->Modify();
	Montage->SlotAnimTracks[Index].SlotName = FName(*NewSlotName);
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}

bool FMontageEditor::AddSlot(UAnimMontage* Montage, const FString& SlotName, FString& OutError)
{
	if (!Montage) { OutError = TEXT("Null montage"); return false; }
	if (SlotName.IsEmpty()) { OutError = TEXT("slot_name is empty"); return false; }
	if (FindSlotIndex(Montage, SlotName) != INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("Slot '%s' already exists"), *SlotName);
		return false;
	}

	Montage->Modify();
	FSlotAnimationTrack NewTrack;
	NewTrack.SlotName = FName(*SlotName);
	Montage->SlotAnimTracks.Add(NewTrack);
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}

bool FMontageEditor::AddSegment(UAnimMontage* Montage, const FString& SlotName, const FString& AnimSequencePath, float StartTime, FString& OutError)
{
	if (!Montage) { OutError = TEXT("Null montage"); return false; }

	const int32 Index = FindSlotIndex(Montage, SlotName);
	if (Index == INDEX_NONE) { OutError = FString::Printf(TEXT("Slot '%s' not found"), *SlotName); return false; }

	UAnimSequence* Sequence = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *AnimSequencePath));
	if (!Sequence) { OutError = FString::Printf(TEXT("AnimSequence not found: %s"), *AnimSequencePath); return false; }

	Montage->Modify();

	FAnimSegment Segment;
	Segment.SetAnimReference(Sequence, true);
	Segment.StartPos = StartTime;

	Montage->SlotAnimTracks[Index].AnimTrack.AnimSegments.Add(Segment);
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}

//-----------------------------------------------------------------------------
// 조회
//-----------------------------------------------------------------------------

FString FMontageEditor::GetInfo(UAnimMontage* Montage)
{
	if (!Montage) return TEXT("{ \"error\": \"null montage\" }");

	FString Out = FString::Printf(TEXT("{ \"montage\": \"%s\", \"slots\": ["), *Montage->GetName());
	for (int32 i = 0; i < Montage->SlotAnimTracks.Num(); ++i)
	{
		const FSlotAnimationTrack& Track = Montage->SlotAnimTracks[i];
		if (i > 0) Out += TEXT(",");
		Out += FString::Printf(TEXT("\n  { \"slot_name\": \"%s\", \"segments\": ["), *Track.SlotName.ToString());
		const TArray<FAnimSegment>& Segments = Track.AnimTrack.AnimSegments;
		for (int32 s = 0; s < Segments.Num(); ++s)
		{
			const UAnimSequenceBase* Anim = Segments[s].GetAnimReference();
			if (s > 0) Out += TEXT(",");
			Out += FString::Printf(TEXT(" { \"anim\": \"%s\", \"start_pos\": %.3f }"),
				Anim ? *Anim->GetName() : TEXT("None"), Segments[s].StartPos);
		}
		Out += TEXT(" ] }");
	}
	Out += TEXT("\n] }");
	return Out;
}
