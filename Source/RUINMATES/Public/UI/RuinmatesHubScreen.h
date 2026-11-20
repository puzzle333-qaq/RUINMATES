#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Hub/RuinmatesHubInteraction.h"
#include "Styling/SlateTypes.h"
#include "RuinmatesHubScreen.generated.h"

class SVerticalBox;
class STextBlock;

/** Event-driven construction/storage terminal, using the shared terminal palette and fonts. */
UCLASS()
class RUINMATES_API URuinmatesHubScreen : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Binds one local session; no client state mutations are performed here. */
	void SetContext(URuinmatesHubInteractionComponent* InRouter, ARuinmatesHubTerminal* InTerminal);
	/** Clears the pending request and displays the authority's actual result. */
	void ShowResult(bool bSuccess, const FText& Reason);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }
private:
	/** Session references are GC tracked and released when the screen closes. */
	UPROPERTY() TObjectPtr<URuinmatesHubInteractionComponent> Router;
	UPROPERTY() TObjectPtr<ARuinmatesHubTerminal> Terminal;
	TSharedPtr<SVerticalBox> Rows;
	FDelegateHandle HubHandle, WarehouseHandle, BackpackHandle;
	bool bPending = false;
	FText Result;
	/** Shared visual resources participate in GC while native Slate retains the screen. */
	UPROPERTY() FSlateBrush PanelBrush;
	UPROPERTY() FButtonStyle CommandStyle;
	TSharedPtr<STextBlock> FuelText;
	int32 LastRevision = -1;
	uint32 LastWarehouseRevision = 0, LastBackpackRevision = 0;
	/** Rebuilds only after inventory/state changes or a command result, never each frame. */
	void Refresh();
	void AddCommand(const FText& Label, ERuinmatesHubCommand Command, FName Id = NAME_None,
		int32 Count = 1, bool bEnabled = true);
	FReply Request(ERuinmatesHubCommand Command, FName Id, int32 Count);
};
