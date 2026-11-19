#pragma once
#include "CoreMinimal.h"
#include "Components/ScrollBox.h"
#include "RuinmatesGridScrollBox.generated.h"

class UScrollBar;

/** Keeps the scrollbar outside the exact five/three-column grid width. */
UCLASS()
class RUINMATES_API URuinmatesGridScrollBox : public UScrollBox
{
	GENERATED_BODY()
public:
	/** Configure before the Slate tree is constructed. */
	void SetExternalScrollbar(UScrollBar* InScrollbar) { ExternalScrollbar = InScrollbar; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	UPROPERTY() TObjectPtr<UScrollBar> ExternalScrollbar;
};
