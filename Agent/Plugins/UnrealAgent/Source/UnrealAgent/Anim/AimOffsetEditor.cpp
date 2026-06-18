#include "Anim/AimOffsetEditor.h"

#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Factories/AimOffsetBlendSpaceFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

//-----------------------------------------------------------------------------
// 로드
//-----------------------------------------------------------------------------

UAimOffsetBlendSpace* FAimOffsetEditor::LoadAimOffset(const FString& AimOffsetPath, FString& OutError)
{
	UObject* Loaded = StaticLoadObject(UAimOffsetBlendSpace::StaticClass(), nullptr, *AimOffsetPath);
	if (!Loaded && !AimOffsetPath.StartsWith(TEXT("/")))
		Loaded = StaticLoadObject(UAimOffsetBlendSpace::StaticClass(), nullptr, *(TEXT("/Game/") + AimOffsetPath));

	UAimOffsetBlendSpace* AimOffset = Cast<UAimOffsetBlendSpace>(Loaded);
	if (!AimOffset)
	{
		OutError = FString::Printf(TEXT("AimOffset not found: %s"), *AimOffsetPath);
		return nullptr;
	}

	const UPackage* Package = AimOffset->GetPackage();
	if (Package && Package->HasAnyPackageFlags(PKG_Cooked))
	{
		OutError = TEXT("Cooked AimOffset cannot be modified.");
		return nullptr;
	}
	return AimOffset;
}

//-----------------------------------------------------------------------------
// 생성
//-----------------------------------------------------------------------------

UAimOffsetBlendSpace* FAimOffsetEditor::CreateAimOffset(const FString& AimOffsetPath, const FString& SkeletonPath, FString& OutError)
{
	if (SkeletonPath.IsEmpty())
	{
		OutError = TEXT("create requires 'skeleton' (AimOffset needs an explicit target skeleton).");
		return nullptr;
	}

	USkeleton* Skeleton = Cast<USkeleton>(StaticLoadObject(USkeleton::StaticClass(), nullptr, *SkeletonPath));
	if (!Skeleton)
	{
		OutError = FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
		return nullptr;
	}

	// 패키지/에셋 이름 분해
	FString PackageName = AimOffsetPath;
	if (!PackageName.StartsWith(TEXT("/")))
		PackageName = TEXT("/Game/") + PackageName;
	const FString AssetName = FPackageName::GetShortName(PackageName);

	if (StaticLoadObject(UAimOffsetBlendSpace::StaticClass(), nullptr, *PackageName))
	{
		OutError = FString::Printf(TEXT("AimOffset already exists: %s"), *PackageName);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package) { OutError = TEXT("Failed to create package"); return nullptr; }

	UAimOffsetBlendSpaceFactoryNew* Factory = NewObject<UAimOffsetBlendSpaceFactoryNew>();
	Factory->TargetSkeleton = Skeleton;

	UAimOffsetBlendSpace* AimOffset = Cast<UAimOffsetBlendSpace>(Factory->FactoryCreateNew(
		UAimOffsetBlendSpace::StaticClass(), Package, FName(*AssetName),
		RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
	if (!AimOffset)
	{
		OutError = TEXT("Factory failed to create AimOffset");
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(AimOffset);
	AimOffset->MarkPackageDirty();
	return AimOffset;
}

//-----------------------------------------------------------------------------
// 샘플 편집
//-----------------------------------------------------------------------------

bool FAimOffsetEditor::AddSample(UAimOffsetBlendSpace* AimOffset, const FString& AnimSequencePath, float Yaw, float Pitch, FString& OutError)
{
	if (!AimOffset) { OutError = TEXT("Null AimOffset"); return false; }

	UAnimSequence* Sequence = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *AnimSequencePath));
	if (!Sequence) { OutError = FString::Printf(TEXT("AnimSequence not found: %s"), *AnimSequencePath); return false; }

	const FVector SampleValue(Yaw, Pitch, 0.f);

	AimOffset->Modify();
	// 좌표가 현재 축 범위를 벗어나면 추가가 거부되므로 범위를 먼저 확장합니다.
	AimOffset->ExpandRangeForSample(SampleValue);
	const int32 NewIndex = AimOffset->AddSample(Sequence, SampleValue);
	if (NewIndex == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("Failed to add sample at (yaw=%.2f, pitch=%.2f). The sequence may be incompatible (e.g. not additive) or overlap an existing sample."), Yaw, Pitch);
		return false;
	}
	AimOffset->PostEditChange();
	AimOffset->MarkPackageDirty();
	return true;
}

//-----------------------------------------------------------------------------
// 조회
//-----------------------------------------------------------------------------

FString FAimOffsetEditor::GetInfo(UAimOffsetBlendSpace* AimOffset)
{
	if (!AimOffset) return TEXT("{ \"error\": \"null aim_offset\" }");

	const FBlendParameter& AxisX = AimOffset->GetBlendParameter(0);
	const FBlendParameter& AxisY = AimOffset->GetBlendParameter(1);

	FString Out = FString::Printf(
		TEXT("{ \"aim_offset\": \"%s\",\n  \"axis_x\": { \"name\": \"%s\", \"min\": %.2f, \"max\": %.2f },\n  \"axis_y\": { \"name\": \"%s\", \"min\": %.2f, \"max\": %.2f },\n  \"samples\": ["),
		*AimOffset->GetName(),
		*AxisX.DisplayName, AxisX.Min, AxisX.Max,
		*AxisY.DisplayName, AxisY.Min, AxisY.Max);

	const TArray<FBlendSample>& Samples = AimOffset->GetBlendSamples();
	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const FBlendSample& Sample = Samples[i];
		if (i > 0) Out += TEXT(",");
		Out += FString::Printf(TEXT("\n    { \"anim\": \"%s\", \"yaw\": %.2f, \"pitch\": %.2f }"),
			Sample.Animation ? *Sample.Animation->GetName() : TEXT("None"),
			Sample.SampleValue.X, Sample.SampleValue.Y);
	}
	Out += TEXT("\n  ] }");
	return Out;
}
