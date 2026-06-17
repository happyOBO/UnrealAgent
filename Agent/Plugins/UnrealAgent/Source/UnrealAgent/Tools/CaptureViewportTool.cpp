#include "Tools/CaptureViewportTool.h"
#include "Editor.h"
#include "UnrealClient.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "Modules/ModuleManager.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CaptureViewportTool)

namespace
{
	// 캡처 이미지의 최대 가로 픽셀. LLM 컨텍스트/전송 크기를 줄이기 위해 이 값으로 다운스케일합니다.
	constexpr int32 MaxWidth = 1024;

	// JPEG 압축 품질(0~100). 화질과 전송 크기의 절충값입니다.
	constexpr int32 JPEGQuality = 70;

	// 종횡비를 유지한 채 InPixels를 OutWidth x OutHeight로 최근접 다운스케일합니다.
	void ResizePixels(const TArray<FColor>& InPixels, int32 InWidth, int32 InHeight,
		TArray<FColor>& OutPixels, int32 OutWidth, int32 OutHeight)
	{
		OutPixels.SetNumUninitialized(OutWidth * OutHeight);

		const float ScaleX = static_cast<float>(InWidth) / OutWidth;
		const float ScaleY = static_cast<float>(InHeight) / OutHeight;

		for (int32 Y = 0; Y < OutHeight; ++Y)
		{
			for (int32 X = 0; X < OutWidth; ++X)
			{
				const int32 SrcX = FMath::Clamp(static_cast<int32>(X * ScaleX), 0, InWidth - 1);
				const int32 SrcY = FMath::Clamp(static_cast<int32>(Y * ScaleY), 0, InHeight - 1);
				OutPixels[Y * OutWidth + X] = InPixels[SrcY * InWidth + SrcX];
			}
		}
	}
}

FMcpResponse FCaptureViewportTool::Execute()
{
	if (!GEditor)
	{
		return FMcpResponse::Failure(TEXT("Editor is not available."));
	}

	// PIE 뷰포트 우선, 없으면 활성 에디터 뷰포트로 폴백합니다.
	FViewport* Viewport = GEditor->GetPIEViewport();
	FString ViewportType = TEXT("PIE");

	if (!Viewport)
	{
		Viewport = GEditor->GetActiveViewport();
		ViewportType = TEXT("Editor");
	}

	if (!Viewport)
	{
		return FMcpResponse::Failure(TEXT("No viewport available. Open a level or start PIE."));
	}

	const FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return FMcpResponse::Failure(TEXT("Viewport has invalid size."));
	}

	TArray<FColor> Pixels;
	if (!Viewport->ReadPixels(Pixels))
	{
		return FMcpResponse::Failure(TEXT("Failed to read viewport pixels."));
	}

	if (Pixels.Num() != ViewportSize.X * ViewportSize.Y)
	{
		return FMcpResponse::Failure(TEXT("Pixel array size mismatch."));
	}

	// 종횡비를 유지하여 폭을 MaxWidth 이하로 다운스케일합니다.
	const int32 OutWidth = FMath::Min(MaxWidth, ViewportSize.X);
	const int32 OutHeight = FMath::Max(1, ViewportSize.Y * OutWidth / ViewportSize.X);

	TArray<FColor> ResizedPixels;
	ResizePixels(Pixels, ViewportSize.X, ViewportSize.Y, ResizedPixels, OutWidth, OutHeight);

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (!ImageWrapper.IsValid())
	{
		return FMcpResponse::Failure(TEXT("Failed to create image wrapper."));
	}

	// FColor는 little-endian에서 BGRA 순서이므로 BGRA로 선언하여 채널 스왑을 방지합니다.
	if (!ImageWrapper->SetRaw(ResizedPixels.GetData(), ResizedPixels.Num() * sizeof(FColor),
		OutWidth, OutHeight, ERGBFormat::BGRA, 8))
	{
		return FMcpResponse::Failure(TEXT("Failed to set image data."));
	}

	TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(JPEGQuality);
	if (CompressedData.Num() == 0)
	{
		return FMcpResponse::Failure(TEXT("Failed to compress image to JPEG."));
	}

	const FString Base64Image = FBase64::Encode(CompressedData.GetData(), CompressedData.Num());

	const FString Caption = FString::Printf(
		TEXT("Captured %s viewport: %dx%d JPEG"), *ViewportType, OutWidth, OutHeight);

	return FMcpResponse::SuccessImage(Base64Image, TEXT("image/jpeg"), Caption);
}
