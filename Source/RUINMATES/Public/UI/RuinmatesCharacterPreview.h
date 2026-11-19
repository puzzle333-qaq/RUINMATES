#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RuinmatesCharacterPreview.generated.h"

class ARuinmatesCharacter;
class UPoseableMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UImage;
class UTextBlock;

/** Cosmetic, non-replicated copy of the real character pose; owns no gameplay components. */
UCLASS()
class RUINMATES_API URuinmatesCharacterPreview : public UUserWidget
{
	GENERATED_BODY()

public:
	/** A null context releases the capture immediately. */
	void SetCharacter(ARuinmatesCharacter* Character);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float Delta) override;

private:
	void ReleaseCapture();
	TWeakObjectPtr<ARuinmatesCharacter> Source;
	UPROPERTY() TObjectPtr<AActor> PreviewActor;
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> PreviewMesh;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Target;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
	UPROPERTY() TObjectPtr<UImage> Image;
	UPROPERTY() TObjectPtr<UTextBlock> Placeholder;
	float CaptureElapsed = 0.f;
};
