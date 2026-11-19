#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "RuinmatesTechButton.generated.h"

class SRuinmatesTechButtonSurface;

/** Native button input with a shared V4 base and independent, interruptible glow. */
UCLASS()
class RUINMATES_API URuinmatesTechButton : public UButton
{
	GENERATED_BODY()

public:
	/** Chooses button/tab artwork without changing the input or content hierarchy. */
	void SetPresentation(bool bInTab, bool bInDanger = false);

	/** Selection is independent of hover, pressed, focus, and enabled state. */
	void SetSelected(bool bInSelected);
	bool IsSelected() const { return bSelected; }

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	bool bSelected = false;
	bool bTab = false;
	bool bDanger = false;
	/** Keeps UObject texture resources alive while Slate's surface references copied brushes. */
	UPROPERTY() TArray<FSlateBrush> RetainedArt;
	TSharedPtr<SRuinmatesTechButtonSurface> Surface;
};
