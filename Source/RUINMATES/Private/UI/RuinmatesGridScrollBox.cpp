#include "UI/RuinmatesGridScrollBox.h"
#include "Components/ScrollBar.h"
#include "Components/ScrollBoxSlot.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScrollBar.h"

TSharedRef<SWidget> URuinmatesGridScrollBox::RebuildWidget()
{
	if (!ExternalScrollbar) return Super::RebuildWidget();
	MyScrollBox = SNew(SScrollBox)
		.Style(&GetWidgetStyle())
		.ScrollBarStyle(&GetWidgetBarStyle())
		.ExternalScrollbar(StaticCastSharedRef<SScrollBar>(ExternalScrollbar->TakeWidget()))
		.Orientation(Orient_Vertical)
		.ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
		.OnUserScrolled(BIND_UOBJECT_DELEGATE(FOnUserScrolled, SlateHandleUserScrolled));
	for (UPanelSlot* PanelSlot : Slots)
	{
		if (UScrollBoxSlot* Typed = Cast<UScrollBoxSlot>(PanelSlot))
		{
			Typed->Parent = this;
			Typed->BuildSlot(MyScrollBox.ToSharedRef());
		}
	}
	return MyScrollBox.ToSharedRef();
}
