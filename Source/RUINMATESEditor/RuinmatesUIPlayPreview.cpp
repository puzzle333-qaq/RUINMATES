#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "UnrealClient.h"

namespace
{
	TOptional<FIntPoint> PreviousWindowSize;
	TOptional<TEnumAsByte<EPlayNetMode>> PreviousNetMode;
	TOptional<int32> PreviousClientCount;
}

/** Resize the real PIE client area; r.SetRes is ignored by editor play windows. */
static FAutoConsoleCommand GResizeV4CapturePIE(
	TEXT("Ruinmates.Editor.ResizeUICapture"), TEXT("Resize UI capture client area: width height."),
	FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
	{
		if (!GEditor || !GEditor->PlayWorld || Args.Num() != 2) return;
		UGameViewportClient* Viewport = GEditor->PlayWorld->GetGameViewport();
		if (!Viewport) return;
		const int32 W = FMath::Clamp(FCString::Atoi(*Args[0]), 640, 3840);
		const int32 H = FMath::Clamp(FCString::Atoi(*Args[1]), 360, 2160);
		if (TSharedPtr<SWindow> Window = Viewport->GetWindow())
		{
			Window->SetSizeLimits(FWindowSizeLimits().SetMaxWidth(4096.f).SetMaxHeight(2304.f));
			// Reconcile against the actual Slate area used by the screenshot API,
			// which excludes the title bar and can differ from the render buffer.
			if (TSharedPtr<SViewport> Surface = Viewport->GetGameViewportWidget())
			{
				const FVector2D Current = Surface->GetCachedGeometry().GetAbsoluteSize();
				Window->ReshapeWindow(FVector2D(20,40), FVector2D(Window->GetSizeInScreen()) + FVector2D(W,H) - Current);
			}
		}
	}));

/** Capture only the game viewport, including Slate, without desktop window chrome. */
static FAutoConsoleCommand GShotV4CapturePIE(
	TEXT("Ruinmates.Editor.ShotUI"), TEXT("Capture native game UI to the supplied filename."),
	FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
	{
		if (!GEditor || !GEditor->PlayWorld || Args.Num() != 1) return;
		FScreenshotRequest::RequestScreenshot(Args[0], true, false, false, FIntRect(), true);
	}));

/** Reproducible UI captures need a game viewport, without the editor toolbars in shot showui. */
static FAutoConsoleCommand GStartV4CapturePIE(
	TEXT("Ruinmates.Editor.StartUICapture"),
	TEXT("Start a floating standalone PIE window for UI verification."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		if (!GEditor || GEditor->PlayWorld) return;
		ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
		PreviousWindowSize = FIntPoint(Settings->NewWindowWidth, Settings->NewWindowHeight);
		EPlayNetMode Mode;
		Settings->GetPlayNetMode(Mode);
		PreviousNetMode = Mode;
		int32 Count;
		Settings->GetPlayNumberOfClients(Count);
		PreviousClientCount = Count;
		Settings->NewWindowWidth = 1920;
		Settings->NewWindowHeight = 1080;
		Settings->SetPlayNetMode(PIE_Standalone);
		Settings->SetPlayNumberOfClients(1);
		FRequestPlaySessionParams Params;
		Params.WorldType = EPlaySessionWorldType::PlayInEditor;
		// An unset DestinationSlateViewport explicitly requests a floating game window.
		GEditor->RequestPlaySession(Params);
	}));

static FAutoConsoleCommand GRestoreV4CaptureSettings(
	TEXT("Ruinmates.Editor.RestoreUICaptureSettings"),
	TEXT("Restore transient play settings after UI captures."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
		if (PreviousWindowSize)
		{
			Settings->NewWindowWidth = PreviousWindowSize->X;
			Settings->NewWindowHeight = PreviousWindowSize->Y;
			PreviousWindowSize.Reset();
		}
		if (PreviousNetMode) { Settings->SetPlayNetMode(PreviousNetMode.GetValue()); PreviousNetMode.Reset(); }
		if (PreviousClientCount) { Settings->SetPlayNumberOfClients(PreviousClientCount.GetValue()); PreviousClientCount.Reset(); }
	}));
