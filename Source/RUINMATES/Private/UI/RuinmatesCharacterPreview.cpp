#include "UI/RuinmatesCharacterPreview.h"

#include "Blueprint/WidgetTree.h"
#include "Character/RuinmatesCharacter.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/RuinmatesUIStyle.h"

void URuinmatesCharacterPreview::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	Image = WidgetTree->ConstructWidget<UImage>();
	if (UOverlaySlot* ImageSlot = Root->AddChildToOverlay(Image))
	{
		ImageSlot->SetHorizontalAlignment(HAlign_Fill);
		ImageSlot->SetVerticalAlignment(VAlign_Fill);
	}
	Placeholder = WidgetTree->ConstructWidget<UTextBlock>();
	Placeholder->SetFont(RuinmatesUI::Font(14));
	Placeholder->SetColorAndOpacity(RuinmatesUI::TextSecondary());
	Placeholder->SetText(NSLOCTEXT("RuinMatesUI", "PreviewUnavailable", "角色预览\n未提供角色"));
	Placeholder->SetJustification(ETextJustify::Center);
	if (UOverlaySlot* PreviewSlot = Root->AddChildToOverlay(Placeholder))
	{
		PreviewSlot->SetHorizontalAlignment(HAlign_Center);
		PreviewSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void URuinmatesCharacterPreview::ReleaseCapture()
{
	if (IsValid(PreviewActor)) PreviewActor->Destroy();
	PreviewActor = nullptr;
	PreviewMesh = nullptr;
	Capture = nullptr;
	Target = nullptr;
	Material = nullptr;
	Source = nullptr;
	if (Image) Image->SetBrush(FSlateBrush());
}

void URuinmatesCharacterPreview::SetCharacter(ARuinmatesCharacter* Character)
{
	if (Source.Get() == Character && IsValid(PreviewActor)) return;
	ReleaseCapture();
	if (Placeholder) Placeholder->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!IsValid(Character) || !GetWorld() || !FApp::CanEverRender() || IsRunningCommandlet()) return;
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!IsValid(Mesh) || !IsValid(Mesh->GetSkeletalMeshAsset())) return;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RUINMATES/UI/AssetPackV4/Materials/M_UI_CharacterPreview.M_UI_CharacterPreview"),
		nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!IsValid(Base)) return;

	FActorSpawnParameters Params;
	Params.ObjectFlags = RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector(0.f, 0.f, -10000.f), FRotator::ZeroRotator, Params);
	if (!IsValid(PreviewActor)) return;
	PreviewActor->SetReplicates(false);
	PreviewActor->SetActorEnableCollision(false);
	PreviewMesh = NewObject<UPoseableMeshComponent>(PreviewActor);
	PreviewActor->SetRootComponent(PreviewMesh);
	PreviewMesh->SetSkinnedAssetAndUpdate(Mesh->GetSkeletalMeshAsset());
	for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
	{
		if (IsValid(Mesh->GetMaterial(Index))) PreviewMesh->SetMaterial(Index, Mesh->GetMaterial(Index));
	}
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetLightingChannels(false, false, true);
	PreviewMesh->SetVisibleInSceneCaptureOnly(true);
	PreviewMesh->RegisterComponent();
	PreviewMesh->SetWorldLocation(FVector(0.f, 0.f, -10000.f));
	PreviewMesh->SetWorldRotation(Mesh->GetRelativeRotation());
	PreviewMesh->CopyPoseFromSkeletalComponent(Mesh);
	UDirectionalLightComponent* KeyLight = NewObject<UDirectionalLightComponent>(PreviewActor);
	KeyLight->SetIntensity(5.f);
	KeyLight->SetLightingChannels(false, false, true);
	KeyLight->SetCastShadows(false);
	KeyLight->RegisterComponent();
	KeyLight->SetWorldRotation(FRotator(-25.f, 210.f, 0.f));

	Target = NewObject<UTextureRenderTarget2D>(this);
	Target->ClearColor = FLinearColor::Transparent;
	Target->RenderTargetFormat = RTF_RGBA16f;
	Target->InitAutoFormat(528, 1040);
	Capture = NewObject<USceneCaptureComponent2D>(PreviewActor);
	Capture->SetupAttachment(PreviewMesh);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Capture->ShowOnlyComponent(PreviewMesh);
	Capture->TextureTarget = Target;
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->RegisterComponent();
	const FBoxSphereBounds Bounds = PreviewMesh->CalcBounds(PreviewMesh->GetComponentTransform());
	const FVector Focus = Bounds.Origin;
	const float Height = FMath::Max(100.f, Bounds.BoxExtent.Z * 2.2f);
	Capture->OrthoWidth = FMath::Max(Height * 528.f / 1040.f,
		FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.2f);
	const FVector Camera = Focus + FVector(1.f, 0.58f, 0.f).GetSafeNormal() * FMath::Max(400.f, Bounds.SphereRadius * 3.f);
	Capture->SetWorldLocation(Camera);
	Capture->SetWorldRotation((Focus - Camera).Rotation());
	Material = UMaterialInstanceDynamic::Create(Base, this);
	Material->SetTextureParameterValue(TEXT("PreviewTexture"), Target);
	if (Image) Image->SetBrushFromMaterial(Material);
	if (Placeholder) Placeholder->SetVisibility(ESlateVisibility::Collapsed);
	Source = Character;
	Capture->CaptureScene();
}

void URuinmatesCharacterPreview::NativeTick(const FGeometry& Geometry, float Delta)
{
	Super::NativeTick(Geometry, Delta);
	if (!Source.IsValid() || !Capture || !PreviewMesh) return;
	CaptureElapsed += Delta;
	if (CaptureElapsed >= 0.1f && IsVisible())
	{
		CaptureElapsed = 0.f;
		PreviewMesh->CopyPoseFromSkeletalComponent(Source->GetMesh());
		Capture->CaptureScene();
	}
}

void URuinmatesCharacterPreview::NativeDestruct()
{
	ReleaseCapture();
	Super::NativeDestruct();
}
