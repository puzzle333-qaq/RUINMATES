#include "UI/RuinmatesTerminalScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/DataTable.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Game/RuinmatesGameState.h"
#include "Game/RuinmatesPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Interface/Interactable.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Inventory/RuinmatesLootContainer.h"
#include "UI/RuinmatesTerminalItemSlot.h"
#include "UI/RuinmatesUIStyle.h"
#include "UI/RuinmatesTechButton.h"
#include "UI/RuinmatesTechControls.h"
#include "UI/RuinmatesCharacterPreview.h"
#include "UI/RuinmatesSquadRadio.h"
#include "UI/RuinmatesFieldHUD.h"

namespace
{
	// ---- Layout constants, 1920x1080 design units -----------------------------------------
	constexpr float ShellOuterX = 24.f;		// screen edge -> metal shell
	constexpr float ShellOuterY = 16.f;
	constexpr float ContentPadX = 72.f;		// shell edge -> content (44 bezel + 28 safe area)
	constexpr float ContentPadTop = 56.f;
	constexpr float ContentPadBottom = 48.f;
	constexpr float PanelGap = 24.f;
	constexpr float PanelPadding = 28.f;
	constexpr float CellGap = 14.f;			// gap between cells (grid slot padding is half of this)
	constexpr float ScrollbarAllowance = 12.f;
	constexpr int32 BackpackColumns = 5;
	constexpr int32 LootColumns = 3;
	constexpr float FadeDuration = 0.16f;
	// Ultrawide: the shell never gets wider than this aspect; the rest stays as dimmed world.
	constexpr float MaxShellAspect = 2.05f;
	TAutoConsoleVariable<int32> LegacyTerminal(TEXT("Ruinmates.UI.LegacyTerminal"), 0,
		TEXT("Use the previous terminal appearance for comparison. Reopen the UI after changing."));

	UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FSlateFontInfo& Font, const FLinearColor& Color, const FText& Text = FText::GetEmpty())
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetText(Text);
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Label;
	}

	UImage* MakeImage(UWidgetTree* Tree, const FName& Name, const FSlateBrush& Brush, const FVector2D& Size, const FLinearColor& Tint = FLinearColor::White)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Image->SetBrush(Brush);
		if (Size.X > 0.f)
		{
			RuinmatesUI::SetImageSize(Image, Size);
		}
		Image->SetColorAndOpacity(Tint);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Image;
	}

	UImage* MakeIcon(UWidgetTree* Tree, const FName& Name, const TCHAR* Texture, float Size, const FLinearColor& Tint)
	{
		return MakeImage(Tree, Name, RuinmatesUI::Image(TEXT("Icons"), Texture), FVector2D(Size, Size), Tint);
	}

	UBorder* MakeBrushBorder(UWidgetTree* Tree, const FName& Name, const FSlateBrush& Brush, const FMargin& Padding)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrush(Brush);
		Border->SetPadding(Padding);
		return Border;
	}

	UHorizontalBoxSlot* AddH(UHorizontalBox* Box, UWidget* Child, const FMargin& Padding = FMargin(0.f), EVerticalAlignment V = VAlign_Center, float FillWeight = 0.f)
	{
		UHorizontalBoxSlot* BoxSlot = Box->AddChildToHorizontalBox(Child);
		if (BoxSlot)
		{
			BoxSlot->SetPadding(Padding);
			BoxSlot->SetVerticalAlignment(V);
			if (FillWeight > 0.f)
			{
				BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				FSlateChildSize Size(ESlateSizeRule::Fill);
				Size.Value = FillWeight;
				BoxSlot->SetSize(Size);
			}
		}
		return BoxSlot;
	}

	UVerticalBoxSlot* AddV(UVerticalBox* Box, UWidget* Child, const FMargin& Padding = FMargin(0.f), bool bFill = false, EHorizontalAlignment H = HAlign_Fill)
	{
		UVerticalBoxSlot* BoxSlot = Box->AddChildToVerticalBox(Child);
		if (BoxSlot)
		{
			BoxSlot->SetPadding(Padding);
			BoxSlot->SetHorizontalAlignment(H);
			if (bFill)
			{
				BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		return BoxSlot;
	}

	UOverlaySlot* AddO(UOverlay* Layer, UWidget* Child, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin(0.f))
	{
		UOverlaySlot* LayerSlot = Layer->AddChildToOverlay(Child);
		if (LayerSlot)
		{
			LayerSlot->SetHorizontalAlignment(H);
			LayerSlot->SetVerticalAlignment(V);
			LayerSlot->SetPadding(Padding);
		}
		return LayerSlot;
	}

	USizeBox* Sized(UWidgetTree* Tree, const FName& Name, UWidget* Child, float Width, float Height)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), Name);
		if (Width > 0.f)
		{
			Box->SetWidthOverride(Width);
		}
		if (Height > 0.f)
		{
			Box->SetHeightOverride(Height);
		}
		Box->SetContent(Child);
		return Box;
	}

	UWidget* MakeDivider(UWidgetTree* Tree, const FName& Name, float Alpha = 1.f)
	{
		return MakeImage(Tree, Name, RuinmatesUI::Image(TEXT("Decorations"), TEXT("T_UI_Divider")), FVector2D(128.f, 2.f),
			FLinearColor(1.f, 1.f, 1.f, Alpha));
	}

	UProgressBar* MakeProgress(UWidgetTree* Tree, const FName& Name)
	{
		UProgressBar* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
		Bar->SetWidgetStyle(RuinmatesUI::ProgressStyle());
		Bar->SetFillColorAndOpacity(RuinmatesUI::Cyan());
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Bar;
	}

	/** Button whose content (label, icons) fills and centres inside the textured brush. */
	UButton* MakeStyledButton(UWidgetTree* Tree, const FName& Name, const FButtonStyle& Style, UWidget* Content, const FMargin& ContentPadding)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(Style);
		Button->AddChild(Content);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Content->Slot))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Center);
			ContentSlot->SetVerticalAlignment(VAlign_Center);
			ContentSlot->SetPadding(ContentPadding);
		}
		return Button;
	}

	/** Small "label over value" block used on the right of panel headers. */
	UVerticalBox* MakeStatBlock(UWidgetTree* Tree, const FName& Name, const FText& Caption, UWidget* Value)
	{
		UVerticalBox* Block = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
		UTextBlock* CaptionText = MakeText(Tree, FName(*(Name.ToString() + TEXT("Caption"))), RuinmatesUI::Font(14), RuinmatesUI::TextSecondary(), Caption);
		CaptionText->SetJustification(ETextJustify::Right);
		AddV(Block, CaptionText, FMargin(0.f), false, HAlign_Right);
		AddV(Block, Value, FMargin(0.f, -2.f, 0.f, 0.f), false, HAlign_Right);
		return Block;
	}

	/** Panel header: tinted icon, title + small english caption, optional stat on the right. */
	UHorizontalBox* MakePanelHeader(UWidgetTree* Tree, const FName& Name, const TCHAR* Icon, UTextBlock* Title, const FText& Caption, UWidget* Right)
	{
		UHorizontalBox* Header = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), Name);
		UBorder* IconPlate = MakeBrushBorder(Tree, FName(*(Name.ToString() + TEXT("IconPlate"))), RuinmatesUI::ChipBrush(), FMargin(8.f));
		IconPlate->SetContent(MakeIcon(Tree, FName(*(Name.ToString() + TEXT("Icon"))), Icon, 32.f, RuinmatesUI::Cyan()));
		IconPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddH(Header, IconPlate, FMargin(0.f, 0.f, 16.f, 0.f));

		UVerticalBox* Titles = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*(Name.ToString() + TEXT("Titles"))));
		Title->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddV(Titles, Title);
		AddV(Titles, MakeText(Tree, FName(*(Name.ToString() + TEXT("Caption"))), RuinmatesUI::FontBold(12, 180), RuinmatesUI::TextWeak(), Caption));
		AddH(Header, Titles, FMargin(0.f), VAlign_Center, 1.f);
		if (Right)
		{
			AddH(Header, Right, FMargin(16.f, 0.f, 0.f, 0.f), VAlign_Bottom);
		}
		return Header;
	}

	/** Equipment slot placeholder: inset plate, glyph, slot name and a status line. */
	UWidget* MakeEquipSlot(UWidgetTree* Tree, const FName& Name, const TCHAR* Icon, const FText& Label, const FText& Status, UTextBlock** OutStatus)
	{
		UBorder* Plate = MakeBrushBorder(Tree, Name, RuinmatesUI::InsetBrush(), FMargin(12.f, 10.f));
		Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
		UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*(Name.ToString() + TEXT("Col"))));
		AddV(Column, MakeIcon(Tree, FName(*(Name.ToString() + TEXT("Icon"))), Icon, 44.f, RuinmatesUI::WithAlpha(RuinmatesUI::TextSecondary(), 0.75f)),
			FMargin(0.f, 2.f, 0.f, 6.f), false, HAlign_Center);
		UTextBlock* LabelText = MakeText(Tree, FName(*(Name.ToString() + TEXT("Label"))), RuinmatesUI::FontBold(16), RuinmatesUI::TextPrimary(), Label);
		LabelText->SetJustification(ETextJustify::Center);
		AddV(Column, LabelText, FMargin(0.f), false, HAlign_Center);
		UTextBlock* StatusText = MakeText(Tree, FName(*(Name.ToString() + TEXT("Status"))), RuinmatesUI::Font(14), RuinmatesUI::TextWeak(), Status);
		StatusText->SetJustification(ETextJustify::Center);
		StatusText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddV(Column, StatusText, FMargin(0.f, 2.f, 0.f, 0.f), false, HAlign_Fill);
		Plate->SetContent(Column);
		if (OutStatus)
		{
			*OutStatus = StatusText;
		}
		return Sized(Tree, FName(*(Name.ToString() + TEXT("Size"))), Plate, 148.f, 132.f);
	}

	/** Uses the data-table enum name as the stable UI filter key. */
	FName TabCategory(ERuinmatesItemCategory Category)
	{
		const UEnum* CategoryEnum = StaticEnum<ERuinmatesItemCategory>();
		return CategoryEnum
			? FName(*CategoryEnum->GetNameStringByValue(static_cast<int64>(Category)))
			: NAME_None;
	}

	/** Display-only lookup of DT_Items: icon, category, description. Never used for authority. */
	void FillFromDatabase(const UObject* Context, const FRuinmatesItemStack& Stack, FRuinmatesTerminalSlotView& View)
	{
		View.Category = Stack.bUpgradeMaterial ? FName(TEXT("Material")) : NAME_None;
		View.Description = Stack.Description;
		View.UnitValue = Stack.UnitValue;
		if (const URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(Context))
		{
			const FName ResolvedId = Database->ResolveItemId(Stack.ItemId, Stack.DisplayName);
			const FRuinmatesItemRow* Row = Database->FindItemDefinition(ResolvedId);
			if (Row)
			{
				// The data table is authoritative for artwork: DT_Items binds the per-item icons.
				// A missing/empty icon keeps the neutral placeholder fallback in the item cell.
				View.Icon = Row->Icon.ToSoftObjectPath();
				View.Category = TabCategory(Row->Category);
				if (View.Description.IsEmpty())
				{
					View.Description = Row->Description;
				}
			}
		}
	}
}

void URuinmatesTerminalScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildChrome();
}

void URuinmatesTerminalScreen::NativeConstruct()
{
	Super::NativeConstruct();
	TArray<UUserWidget*> Overlays;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Overlays, URuinmatesFieldHUD::StaticClass(), true);
	for (UUserWidget* Overlay : Overlays)
	{
		if (Overlay->GetOwningPlayer() != GetOwningPlayer()) continue;
		HiddenOverlays.Add(Overlay, Overlay->GetVisibility());
		Overlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URuinmatesTerminalScreen::NativeDestruct()
{
	CloseSlotMenu();
	UnbindSources();
	if (CharacterPreview) CharacterPreview->SetCharacter(nullptr);
	for (const auto& Overlay : HiddenOverlays)
	{
		if (Overlay.Key.IsValid()) Overlay.Key->SetVisibility(Overlay.Value);
	}
	HiddenOverlays.Empty();
	Super::NativeDestruct();
}

FReply URuinmatesTerminalScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && ContextMenu && ContextMenu->GetVisibility() == ESlateVisibility::Visible)
	{
		CloseSlotMenu();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Tab)
	{
		ClosePreview();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply URuinmatesTerminalScreen::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Tab)
	{
		return NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void URuinmatesTerminalScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Open fade: 160 ms, then this branch is idle.
	if (FadeTime < FadeDuration)
	{
		FadeTime = FMath::Min(FadeDuration, FadeTime + InDeltaTime);
		const float Alpha = FadeTime / FadeDuration;
		SetRenderOpacity(Alpha);
		if (ShellRoot)
		{
			ShellRoot->SetRenderTranslation(FVector2D(0.f, (1.f - Alpha) * 8.f));
		}
	}

	// Both checks are a float compare per frame; they only do work when the geometry changed.
	UpdateShellBounds(MyGeometry);
	UpdateCellSizes(false);
	if (DesignCanvas && RightSwitcher && PageFadeTime < 0.2f)
	{
		PageFadeTime = FMath::Min(0.2f, PageFadeTime + InDeltaTime);
		const float Alpha = 1.f - FMath::Square(1.f - PageFadeTime / 0.2f);
		RightSwitcher->SetRenderOpacity(Alpha);
		RightSwitcher->SetRenderTranslation(FVector2D(4.f * (1.f - Alpha), 0.f));
	}
	if (BackpackGrid && FilterFadeTime < 0.12f)
	{
		FilterFadeTime = FMath::Min(0.12f, FilterFadeTime + InDeltaTime);
		const float Alpha = 1.f - FMath::Square(1.f - FilterFadeTime / 0.12f);
		BackpackGrid->SetRenderOpacity(0.76f + 0.24f * Alpha);
	}
}

void URuinmatesTerminalScreen::BeginOpenFade()
{
	FadeTime = 0.f;
	SetRenderOpacity(0.f);
}

void URuinmatesTerminalScreen::UpdateShellBounds(const FGeometry& MyGeometry)
{
	const FVector2D ViewSize = MyGeometry.GetLocalSize();
	if (ViewSize.Equals(LastViewSize, 0.5f) || ViewSize.Y <= 1.f)
	{
		return;
	}
	LastViewSize = ViewSize;
	if (DesignCanvas)
	{
		const float Scale = FMath::Min(ViewSize.X / 1920.f, ViewSize.Y / 1080.f);
		DesignCanvas->SetRenderScale(FVector2D(Scale));
		return;
	}
	if (!ShellRoot) return;
	float SideInset = ShellOuterX;
	const float MaxWidth = ViewSize.Y * MaxShellAspect;
	if (ViewSize.X > MaxWidth)
	{
		SideInset = (ViewSize.X - MaxWidth) * 0.5f;
	}
	if (UOverlaySlot* ShellSlot = Cast<UOverlaySlot>(ShellRoot->Slot))
	{
		ShellSlot->SetPadding(FMargin(SideInset, ShellOuterY));
	}
}

void URuinmatesTerminalScreen::UpdateCellSizes(bool bForce)
{
	if (DesignCanvas)
	{
		if (bForce)
		{
			for (URuinmatesTerminalItemSlot* Cell : BackpackSlots)
			{
				if (Cell) Cell->Configure(FVector2D(168.f, 112.f), ERuinmatesTerminalSlotLayout::Grid);
			}
			for (URuinmatesTerminalItemSlot* Cell : LootSlots)
			{
				if (Cell) Cell->Configure(FVector2D(168.f, 112.f), ERuinmatesTerminalSlotLayout::Grid);
			}
		}
		return;
	}
	// Cell width = (scroll width - scrollbar - columns * gap) / columns, from the arranged geometry.
	if (BackpackScroll)
	{
		const float Width = BackpackScroll->GetCachedGeometry().GetLocalSize().X;
		if (Width > 1.f && (bForce || FMath::Abs(Width - LastBackpackWidth) > 0.5f))
		{
			LastBackpackWidth = Width;
			const float Cell = FMath::Clamp(FMath::FloorToFloat((Width - ScrollbarAllowance - BackpackColumns * CellGap) / BackpackColumns), 96.f, 200.f);
			for (URuinmatesTerminalItemSlot* Cell_ : BackpackSlots)
			{
				if (Cell_)
				{
					Cell_->Configure(FVector2D(Cell, Cell), ERuinmatesTerminalSlotLayout::Grid);
				}
			}
		}
	}
	if (LootScroll)
	{
		const float Width = LootScroll->GetCachedGeometry().GetLocalSize().X;
		if (Width > 1.f && (bForce || FMath::Abs(Width - LastLootWidth) > 0.5f))
		{
			LastLootWidth = Width;
			const float CellW = FMath::Clamp(FMath::FloorToFloat((Width - ScrollbarAllowance - LootColumns * CellGap) / LootColumns), 120.f, 224.f);
			const float CellH = FMath::RoundToFloat(CellW * 1.23f);
			for (URuinmatesTerminalItemSlot* Cell_ : LootSlots)
			{
				if (Cell_)
				{
					Cell_->Configure(FVector2D(CellW, CellH), ERuinmatesTerminalSlotLayout::Card);
				}
			}
		}
	}
}

void URuinmatesTerminalScreen::BuildChrome()
{
	if (LegacyTerminal.GetValueOnGameThread() == 0)
	{
		BuildV4Chrome();
		return;
	}
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WBP_TerminalRoot"));
	WidgetTree->RootWidget = Canvas;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootLayer"));
	if (UCanvasPanelSlot* RootSlot = Canvas->AddChildToCanvas(Root))
	{
		RootSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		RootSlot->SetOffsets(FMargin(0.f));
	}

	// 1) One light blur plus a dimmer so the device reads in front of the world.
	UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("WorldBlur"));
	Blur->SetBlurStrength(6.f);
	Blur->SetApplyAlphaToBlur(true);
	Blur->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddO(Root, Blur, HAlign_Fill, VAlign_Fill);
	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WorldDimmer"));
	Dimmer->SetBrushColor(RuinmatesUI::Token(TEXT("040B14"), 0.55f));
	Dimmer->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddO(Root, Dimmer, HAlign_Fill, VAlign_Fill);

	// 2) The device: glass screen, noise, content, then the metal bezel on top (display only).
	UOverlay* Shell = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DeviceShell"));
	ShellRoot = Shell;
	AddO(Root, Shell, HAlign_Fill, VAlign_Fill, FMargin(ShellOuterX, ShellOuterY));

	UImage* Glass = MakeImage(WidgetTree, TEXT("ShellBackground"),
		RuinmatesUI::Box(TEXT("Frame"), TEXT("T_UI_ScreenGlass"), FMargin(0.25f)), FVector2D::ZeroVector);
	AddO(Shell, Glass, HAlign_Fill, VAlign_Fill, FMargin(36.f));
	UImage* Noise = MakeImage(WidgetTree, TEXT("ScreenNoise"), RuinmatesUI::Tiled(TEXT("Panels"), TEXT("T_UI_PanelNoise")),
		FVector2D::ZeroVector, FLinearColor(1.f, 1.f, 1.f, 0.55f));
	AddO(Shell, Noise, HAlign_Fill, VAlign_Fill, FMargin(40.f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SafeContent"));
	AddO(Shell, Column, HAlign_Fill, VAlign_Fill, FMargin(ContentPadX, ContentPadTop, ContentPadX, ContentPadBottom));

	UImage* Bezel = MakeImage(WidgetTree, TEXT("DeviceFrame"),
		RuinmatesUI::Box(TEXT("Frame"), TEXT("T_UI_TerminalFrame"), FMargin(120.f / 320.f)), FVector2D::ZeroVector);
	AddO(Shell, Bezel, HAlign_Fill, VAlign_Fill);
	UImage* TopPlate = MakeImage(WidgetTree, TEXT("FrameTopPlate"), RuinmatesUI::Image(TEXT("Frame"), TEXT("T_UI_FrameTopPlate")), FVector2D(560.f, 30.f));
	AddO(Shell, TopPlate, HAlign_Center, VAlign_Top, FMargin(0.f, 4.f, 0.f, 0.f));
	UImage* BottomPlate = MakeImage(WidgetTree, TEXT("FrameBottomPlate"), RuinmatesUI::Image(TEXT("Frame"), TEXT("T_UI_FrameTopPlate")),
		FVector2D(420.f, 22.f), FLinearColor(0.8f, 0.8f, 0.85f, 1.f));
	BottomPlate->SetRenderTransformAngle(180.f);
	AddO(Shell, BottomPlate, HAlign_Center, VAlign_Bottom, FMargin(0.f, 0.f, 0.f, 4.f));
	AddO(Shell, MakeImage(WidgetTree, TEXT("ClampL"), RuinmatesUI::Image(TEXT("Frame"), TEXT("T_UI_FrameSideClamp_L")), FVector2D(22.f, 220.f)),
		HAlign_Left, VAlign_Center, FMargin(-8.f, 0.f, 0.f, 0.f));
	AddO(Shell, MakeImage(WidgetTree, TEXT("ClampR"), RuinmatesUI::Image(TEXT("Frame"), TEXT("T_UI_FrameSideClamp_R")), FVector2D(22.f, 220.f)),
		HAlign_Right, VAlign_Center, FMargin(0.f, 0.f, -8.f, 0.f));

	BuildHeader(Column);
	BuildTabs(Column);
	BuildBody(Column);
	BuildFooter(Column);

	BuildMenuLayer(Canvas);
}

void URuinmatesTerminalScreen::BuildHeader(UVerticalBox* Column)
{
	UOverlay* Header = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("WBP_TerminalHeader"));
	AddV(Column, Sized(WidgetTree, TEXT("HeaderSize"), Header, 0.f, 84.f));

	// Left: logo + brand block.
	UHorizontalBox* Brand = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BrandBlock"));
	AddH(Brand, MakeIcon(WidgetTree, TEXT("Logo"), TEXT("T_UI_Icon_Logo"), 56.f, FLinearColor::White), FMargin(0.f, 0.f, 14.f, 0.f));
	UVerticalBox* BrandText = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BrandText"));
	AddV(BrandText, MakeText(WidgetTree, TEXT("Brand"), RuinmatesUI::FontBold(24, 220), RuinmatesUI::TextPrimary(),
		NSLOCTEXT("RuinMatesUI", "Brand", "RUINMATES")));
	AddV(BrandText, MakeText(WidgetTree, TEXT("BrandSub"), RuinmatesUI::Font(15), RuinmatesUI::TextSecondary(),
		NSLOCTEXT("RuinMatesUI", "BrandSub2", "个人终端  ·  PERSONAL TERMINAL")));
	AddH(Brand, BrandText);
	AddO(Header, Brand, HAlign_Left, VAlign_Center);

	// Center: page title between two technical rules.
	UHorizontalBox* Title = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleBlock"));
	AddH(Title, MakeImage(WidgetTree, TEXT("TitleDecoL"), RuinmatesUI::Image(TEXT("Decorations"), TEXT("T_UI_TitleDeco_L")), FVector2D(200.f, 17.f)),
		FMargin(0.f, 0.f, 20.f, 0.f));
	PageTitle = MakeText(WidgetTree, TEXT("PageTitle"), RuinmatesUI::FontBold(42, 120), RuinmatesUI::TextPrimary(),
		NSLOCTEXT("RuinMatesUI", "PageInventory", "背包"));
	PageTitle->SetShadowOffset(FVector2D(0.f, 2.f));
	PageTitle->SetShadowColorAndOpacity(RuinmatesUI::WithAlpha(RuinmatesUI::Cyan(), 0.25f));
	AddH(Title, PageTitle);
	AddH(Title, MakeImage(WidgetTree, TEXT("TitleDecoR"), RuinmatesUI::Image(TEXT("Decorations"), TEXT("T_UI_TitleDeco_R")), FVector2D(200.f, 17.f)),
		FMargin(20.f, 0.f, 0.f, 0.f));
	AddO(Header, Title, HAlign_Center, VAlign_Center);

	// Right: terminal clock, decorative signal / battery (no such gameplay systems), close.
	UHorizontalBox* Status = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StatusBlock"));
	UHorizontalBox* ClockRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ClockRow"));
	AddH(ClockRow, MakeIcon(WidgetTree, TEXT("ClockIcon"), TEXT("T_UI_Icon_Clock"), 20.f, RuinmatesUI::Cyan()), FMargin(0.f, 0.f, 8.f, 0.f));
	ClockText = MakeText(WidgetTree, TEXT("Clock"), RuinmatesUI::FontBold(20), RuinmatesUI::TextPrimary(),
		FText::FromString(FDateTime::Now().ToString(TEXT("%H:%M"))));
	AddH(ClockRow, ClockText);
	UBorder* ClockChip = MakeBrushBorder(WidgetTree, TEXT("ClockChip"), RuinmatesUI::ChipBrush(), FMargin(14.f, 7.f));
	ClockChip->SetContent(ClockRow);
	AddH(Status, ClockChip);
	AddH(Status, MakeIcon(WidgetTree, TEXT("SignalIcon"), TEXT("T_UI_Icon_Signal"), 24.f, RuinmatesUI::Cyan()), FMargin(22.f, 0.f, 0.f, 0.f));
	AddH(Status, MakeIcon(WidgetTree, TEXT("BatteryIcon"), TEXT("T_UI_Icon_Battery"), 28.f, RuinmatesUI::Success()), FMargin(18.f, 0.f, 6.f, 0.f));
	AddH(Status, MakeText(WidgetTree, TEXT("BatteryText"), RuinmatesUI::Font(17), RuinmatesUI::TextSecondary(), FText::FromString(TEXT("87%"))));

	CloseButton = MakeStyledButton(WidgetTree, TEXT("Close"), RuinmatesUI::CloseButtonStyle(),
		MakeIcon(WidgetTree, TEXT("CloseIcon"), TEXT("T_UI_Icon_Close"), 22.f, RuinmatesUI::TextPrimary()), FMargin(0.f));
	CloseButton->SetToolTipText(NSLOCTEXT("RuinMatesUI", "CloseTip", "关闭终端（ESC）"));
	CloseButton->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::ClosePreview);
	AddH(Status, Sized(WidgetTree, TEXT("CloseSize"), CloseButton, 48.f, 48.f), FMargin(22.f, 0.f, 0.f, 0.f));
	AddO(Header, Status, HAlign_Right, VAlign_Center);

	AddV(Column, MakeDivider(WidgetTree, TEXT("HeaderRule")), FMargin(0.f, 8.f, 0.f, 14.f));
}

void URuinmatesTerminalScreen::BuildTabs(UVerticalBox* Column)
{
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("WBP_TerminalTabs"));

	struct FTabDef
	{
		FName Filter;
		FText Label;
		const TCHAR* Icon;
	};

	const FTabDef Definitions[] =
	{
		{ NAME_None, NSLOCTEXT("RuinMatesUI", "TabAll", "全部物资"), TEXT("T_UI_Icon_TabAll") },
		{ FName(TEXT("Consumable")), NSLOCTEXT("RuinMatesUI", "TabConsumable", "消耗品"), TEXT("T_UI_Icon_TabConsumable") },
		{ FName(TEXT("Material")), NSLOCTEXT("RuinMatesUI", "TabMaterial", "材料"), TEXT("T_UI_Icon_TabMaterial") },
		{ FName(TEXT("Quest")), NSLOCTEXT("RuinMatesUI", "TabQuest", "任务物品"), TEXT("T_UI_Icon_TabQuest") },
	};

	TabButtons.Reset();
	TabLabels.Reset();
	TabIcons.Reset();
	TabFilters.Reset();

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Definitions); ++Index)
	{
		const FTabDef& Definition = Definitions[Index];
		UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("TabContent_%d"), Index)));
		UImage* Icon = MakeIcon(WidgetTree, FName(*FString::Printf(TEXT("TabIcon_%d"), Index)), Definition.Icon, 24.f, RuinmatesUI::TextSecondary());
		AddH(Content, Icon, FMargin(0.f, 0.f, 10.f, 0.f));
		UTextBlock* Label = MakeText(WidgetTree, FName(*FString::Printf(TEXT("TabLabel_%d"), Index)), RuinmatesUI::FontBold(22), RuinmatesUI::TextSecondary(), Definition.Label);
		AddH(Content, Label);

		UButton* Tab = MakeStyledButton(WidgetTree, FName(*FString::Printf(TEXT("TabButton_%d"), Index)), RuinmatesUI::TabStyle(false), Content, FMargin(0.f, 0.f, 0.f, 2.f));
		if (Definition.Filter.IsNone())
		{
			Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectAll);
		}
		else if (Definition.Filter == TEXT("Consumable"))
		{
			Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectConsumable);
		}
		else if (Definition.Filter == TEXT("Material"))
		{
			Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectMaterial);
		}
		else
		{
			Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectQuest);
		}
		AddH(Tabs, Sized(WidgetTree, FName(*FString::Printf(TEXT("TabSize_%d"), Index)), Tab, 204.f, 58.f), FMargin(0.f, 0.f, 12.f, 0.f));

		TabButtons.Add(Tab);
		TabLabels.Add(Label);
		TabIcons.Add(Icon);
		TabFilters.Add(Definition.Filter);
	}

	// Team wallet on the right of the tab row (real ARuinmatesGameState data; hidden when absent).
	AddH(Tabs, WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("TabSpacer")), FMargin(0.f), VAlign_Center, 1.f);
	UHorizontalBox* Funds = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FundsRow"));
	AddH(Funds, MakeIcon(WidgetTree, TEXT("FundsIcon"), TEXT("T_UI_Icon_Currency"), 30.f, FLinearColor::White), FMargin(0.f, 0.f, 10.f, 0.f));
	AddH(Funds, MakeText(WidgetTree, TEXT("FundsCaption"), RuinmatesUI::Font(16), RuinmatesUI::TextSecondary(),
		NSLOCTEXT("RuinMatesUI", "FundsCaption", "团队资金")), FMargin(0.f, 0.f, 14.f, 0.f));
	FundsText = MakeText(WidgetTree, TEXT("Funds"), RuinmatesUI::FontBold(24), RuinmatesUI::Money(), FText::AsNumber(0));
	AddH(Funds, FundsText);
	UBorder* FundsBorder = MakeBrushBorder(WidgetTree, TEXT("FundsChip"), RuinmatesUI::ChipBrush(), FMargin(18.f, 8.f, 22.f, 8.f));
	FundsBorder->SetContent(Funds);
	FundsBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	FundsChip = FundsBorder;
	AddH(Tabs, FundsBorder);

	AddV(Column, Tabs, FMargin(0.f, 0.f, 0.f, 18.f));
}

void URuinmatesTerminalScreen::BuildBody(UVerticalBox* Column)
{
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("WBP_InventoryScreen"));
	AddV(Column, Body, FMargin(0.f), true);

	// ---- Left: personal backpack (fixed in both modes) ----------------------------------
	UBorder* Left = MakeBrushBorder(WidgetTree, TEXT("WBP_BackpackPanel"), RuinmatesUI::PanelBrush(), FMargin(PanelPadding, 24.f, PanelPadding, 20.f));
	AddH(Body, Left, FMargin(0.f, 0.f, PanelGap, 0.f), VAlign_Fill, 0.59f);
	UVerticalBox* LeftColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BackpackColumn"));
	Left->SetContent(LeftColumn);

	PackTitle = MakeText(WidgetTree, TEXT("PackTitle"), RuinmatesUI::FontBold(30), RuinmatesUI::TextPrimary(), NSLOCTEXT("RuinMatesUI", "PackIssue", "免费制式背包"));
	UHorizontalBox* CapacityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CapacityRow"));
	CapacityUsedText = MakeText(WidgetTree, TEXT("CapacityUsed"), RuinmatesUI::FontBold(30), RuinmatesUI::TextPrimary(), FText::AsNumber(0));
	AddH(CapacityRow, CapacityUsedText, FMargin(0.f), VAlign_Bottom);
	CapacityText = MakeText(WidgetTree, TEXT("CapacityText"), RuinmatesUI::Font(22), RuinmatesUI::TextSecondary(), FText::FromString(TEXT(" / 10")));
	AddH(CapacityRow, CapacityText, FMargin(0.f, 0.f, 0.f, 3.f), VAlign_Bottom);
	AddV(LeftColumn, MakePanelHeader(WidgetTree, TEXT("WBP_PanelHeader"), TEXT("T_UI_Icon_Backpack"), PackTitle,
		NSLOCTEXT("RuinMatesUI", "PackCaption", "PERSONAL BACKPACK"),
		MakeStatBlock(WidgetTree, TEXT("CapacityStat"), NSLOCTEXT("RuinMatesUI", "CapacityCaption", "容量"), CapacityRow)));

	CapacityBar = MakeProgress(WidgetTree, TEXT("WBP_CapacityBar"));
	AddV(LeftColumn, Sized(WidgetTree, TEXT("CapacityBarSize"), CapacityBar, 0.f, 14.f), FMargin(0.f, 14.f, 0.f, 18.f));

	BackpackScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BackpackScroll"));
	BackpackScroll->SetWidgetBarStyle(RuinmatesUI::ScrollBarStyle());
	BackpackScroll->SetScrollbarThickness(FVector2D(4.f, 4.f));
	BackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("BackpackGrid"));
	BackpackGrid->SetSlotPadding(FMargin(CellGap * 0.5f));
	BackpackScroll->AddChild(BackpackGrid);
	AddV(LeftColumn, BackpackScroll, FMargin(-CellGap * 0.5f, 0.f), true);

	AddV(LeftColumn, MakeDivider(WidgetTree, TEXT("ValueRule"), 0.8f), FMargin(0.f, 10.f, 0.f, 12.f));
	UHorizontalBox* ValueRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("WBP_BackpackSummary"));
	AddH(ValueRow, MakeIcon(WidgetTree, TEXT("ValueIcon"), TEXT("T_UI_Icon_Currency"), 30.f, FLinearColor::White), FMargin(0.f, 0.f, 10.f, 0.f));
	AddH(ValueRow, MakeText(WidgetTree, TEXT("ValueCaption"), RuinmatesUI::Font(18), RuinmatesUI::TextSecondary(),
		NSLOCTEXT("RuinMatesUI", "ValueCaption", "物资总价值")), FMargin(0.f), VAlign_Center, 1.f);
	ValueText = MakeText(WidgetTree, TEXT("ValueText"), RuinmatesUI::FontBold(28), RuinmatesUI::Money(), FText::AsNumber(0));
	AddH(ValueRow, ValueText);
	AddV(LeftColumn, ValueRow);

	// ---- Right: character / container switcher ---------------------------------------------
	UBorder* Right = MakeBrushBorder(WidgetTree, TEXT("RightPanel"), RuinmatesUI::PanelBrush(), FMargin(PanelPadding, 24.f, PanelPadding, 20.f));
	AddH(Body, Right, FMargin(0.f), VAlign_Fill, 0.41f);
	RightSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("RightSwitcher"));
	Right->SetContent(RightSwitcher);

	// Character page.
	UVerticalBox* Equipment = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WBP_EquipmentPanel"));
	RightTitle = MakeText(WidgetTree, TEXT("CharacterName"), RuinmatesUI::FontBold(30), RuinmatesUI::TextPrimary(), NSLOCTEXT("RuinMatesUI", "Survivor", "幸存者"));
	RightSubtitle = MakeText(WidgetTree, TEXT("CharacterLevel"), RuinmatesUI::FontBold(20), RuinmatesUI::Cyan(), FText::FromString(TEXT("LV.1")));
	UBorder* LevelChip = MakeBrushBorder(WidgetTree, TEXT("LevelChip"), RuinmatesUI::ChipBrush(), FMargin(14.f, 5.f));
	LevelChip->SetContent(RightSubtitle);
	AddV(Equipment, MakePanelHeader(WidgetTree, TEXT("CharacterHeader"), TEXT("T_UI_Icon_EquipArmor"), RightTitle,
		NSLOCTEXT("RuinMatesUI", "OperatorCaption", "OPERATOR PROFILE"), LevelChip));

	UHorizontalBox* Stage = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EquipStage"));
	AddV(Equipment, Stage, FMargin(0.f, 18.f, 0.f, 0.f), true);

	UVerticalBox* LeftSlots = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EquipLeft"));
	UVerticalBox* RightSlots = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EquipRight"));
	const FText Locked = NSLOCTEXT("RuinMatesUI", "EquipLocked", "暂未开放");
	auto AddSlot = [this, Locked](UVerticalBox* Side, const TCHAR* Name, const TCHAR* Icon, const FText& Label, UTextBlock** OutStatus, bool bLast)
	{
		UTextBlock* StatusText = nullptr;
		UWidget* EquipSlot = MakeEquipSlot(WidgetTree, Name, Icon, Label, Locked, &StatusText);
		UVerticalBoxSlot* BoxSlot = AddV(Side, EquipSlot, FMargin(0.f), false, HAlign_Center);
		if (!bLast)
		{
			AddV(Side, WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName(*(FString(Name) + TEXT("Gap")))), FMargin(0.f), true);
		}
		if (OutStatus)
		{
			*OutStatus = StatusText;
		}
		return BoxSlot;
	};
	AddSlot(LeftSlots, TEXT("EquipHead"), TEXT("T_UI_Icon_EquipHead"), NSLOCTEXT("RuinMatesUI", "EquipHead", "头部"), nullptr, false);
	AddSlot(LeftSlots, TEXT("EquipArmor"), TEXT("T_UI_Icon_EquipArmor"), NSLOCTEXT("RuinMatesUI", "EquipArmor", "护甲"), nullptr, false);
	UTextBlock* BackpackStatus = nullptr;
	UTextBlock* PrimaryStatus = nullptr;
	AddSlot(LeftSlots, TEXT("EquipBackpack"), TEXT("T_UI_Icon_EquipBackpack"), NSLOCTEXT("RuinMatesUI", "EquipBackpackLabel", "背包"), &BackpackStatus, true);
	AddSlot(RightSlots, TEXT("EquipPrimary"), TEXT("T_UI_Icon_EquipPrimary"), NSLOCTEXT("RuinMatesUI", "EquipPrimary", "主武器"), &PrimaryStatus, false);
	EquipBackpackText = BackpackStatus;
	EquipPrimaryText = PrimaryStatus;
	AddSlot(RightSlots, TEXT("EquipSecondary"), TEXT("T_UI_Icon_EquipSecondary"), NSLOCTEXT("RuinMatesUI", "EquipSecondary", "副武器"), nullptr, false);
	AddSlot(RightSlots, TEXT("EquipMelee"), TEXT("T_UI_Icon_EquipMelee"), NSLOCTEXT("RuinMatesUI", "EquipMelee", "近战"), nullptr, true);
	AddH(Stage, LeftSlots, FMargin(0.f), VAlign_Fill);

	// Character stage: recessed plate, platform ring and a hologram silhouette (no SceneCapture yet).
	UOverlay* Preview = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("WBP_CharacterPreview"));
	Preview->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddO(Preview, MakeImage(WidgetTree, TEXT("PreviewPlate"), RuinmatesUI::InsetBrush(), FVector2D::ZeroVector, FLinearColor(1.f, 1.f, 1.f, 0.6f)), HAlign_Fill, VAlign_Fill);
	AddO(Preview, MakeImage(WidgetTree, TEXT("PreviewPlatform"), RuinmatesUI::Image(TEXT("Decorations"), TEXT("T_UI_PreviewPlatform")), FVector2D(300.f, 75.f)),
		HAlign_Center, VAlign_Bottom, FMargin(0.f, 0.f, 0.f, 18.f));
	UScaleBox* SilhouetteScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("SilhouetteScale"));
	SilhouetteScale->SetStretch(EStretch::ScaleToFit);
	SilhouetteScale->AddChild(MakeImage(WidgetTree, TEXT("Silhouette"), RuinmatesUI::Image(TEXT("Decorations"), TEXT("T_UI_CharacterSilhouette")), FVector2D(300.f, 540.f)));
	AddO(Preview, SilhouetteScale, HAlign_Center, VAlign_Fill, FMargin(10.f, 20.f, 10.f, 46.f));
	UTextBlock* PreviewNote = MakeText(WidgetTree, TEXT("PreviewNote"), RuinmatesUI::Font(13), RuinmatesUI::TextWeak(),
		NSLOCTEXT("RuinMatesUI", "PreviewNote", "角色剪影 · 实时预览未接入"));
	AddO(Preview, PreviewNote, HAlign_Center, VAlign_Bottom, FMargin(0.f, 0.f, 0.f, 8.f));
	AddH(Stage, Preview, FMargin(14.f, 0.f), VAlign_Fill, 1.f);
	AddH(Stage, RightSlots, FMargin(0.f), VAlign_Fill);
	RightSwitcher->AddChild(Equipment);

	// Container page.
	UVerticalBox* Loot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WBP_LootContainerPanel"));
	LootTitleText = MakeText(WidgetTree, TEXT("LootTitle"), RuinmatesUI::FontBold(30), RuinmatesUI::TextPrimary(), NSLOCTEXT("RuinMatesUI", "LootTitle", "普通储物箱"));
	LootCountText = MakeText(WidgetTree, TEXT("LootCount"), RuinmatesUI::FontBold(30), RuinmatesUI::TextPrimary(), FText::FromString(TEXT("0 / 0")));
	AddV(Loot, MakePanelHeader(WidgetTree, TEXT("LootHeader"), TEXT("T_UI_Icon_Container"), LootTitleText,
		NSLOCTEXT("RuinMatesUI", "LootCaption", "SEARCH CONTAINER"),
		MakeStatBlock(WidgetTree, TEXT("LootStat"), NSLOCTEXT("RuinMatesUI", "SearchedCaption", "已搜索"), LootCountText)));

	SearchBar = MakeProgress(WidgetTree, TEXT("WBP_SearchProgress"));
	AddV(Loot, Sized(WidgetTree, TEXT("SearchBarSize"), SearchBar, 0.f, 14.f), FMargin(0.f, 14.f, 0.f, 10.f));

	UHorizontalBox* SearchStatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SearchStatusRow"));
	AddH(SearchStatusRow, MakeIcon(WidgetTree, TEXT("SearchStatusIcon"), TEXT("T_UI_Icon_Scan"), 20.f, RuinmatesUI::Cyan()), FMargin(0.f, 0.f, 8.f, 0.f));
	SearchStatusText = MakeText(WidgetTree, TEXT("SearchStatus"), RuinmatesUI::Font(16), RuinmatesUI::Cyan());
	AddH(SearchStatusRow, SearchStatusText);
	AddV(Loot, SearchStatusRow, FMargin(0.f, 0.f, 0.f, 12.f));

	LootScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LootScroll"));
	LootScroll->SetWidgetBarStyle(RuinmatesUI::ScrollBarStyle());
	LootScroll->SetScrollbarThickness(FVector2D(4.f, 4.f));
	LootGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("LootGrid"));
	LootGrid->SetSlotPadding(FMargin(CellGap * 0.5f));
	LootScroll->AddChild(LootGrid);
	AddV(Loot, LootScroll, FMargin(-CellGap * 0.5f, 0.f), true);

	UHorizontalBox* TakeContent = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TakeContent"));
	AddH(TakeContent, MakeIcon(WidgetTree, TEXT("TakeIcon"), TEXT("T_UI_Icon_Backpack"), 26.f, RuinmatesUI::TextPrimary()), FMargin(0.f, 0.f, 12.f, 0.f));
	TakeAllLabel = MakeText(WidgetTree, TEXT("TakeLabel"), RuinmatesUI::FontBold(22), RuinmatesUI::TextPrimary(), NSLOCTEXT("RuinMatesUI", "TakeFound", "拾取已发现物品"));
	AddH(TakeContent, TakeAllLabel);
	TakeAllButton = MakeStyledButton(WidgetTree, TEXT("WBP_TerminalButton"), RuinmatesUI::ButtonStyle(), TakeContent, FMargin(0.f));
	TakeAllButton->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleTakeAll);
	AddV(Loot, Sized(WidgetTree, TEXT("TakeSize"), TakeAllButton, 0.f, 62.f), FMargin(0.f, 14.f, 0.f, 0.f));
	RightSwitcher->AddChild(Loot);
}

void URuinmatesTerminalScreen::BuildFooter(UVerticalBox* Column)
{
	UBorder* Footer = MakeBrushBorder(WidgetTree, TEXT("WBP_TerminalFooter"),
		RuinmatesUI::Box(TEXT("Panels"), TEXT("T_UI_FooterBar"), FMargin(0.2f, 0.3f, 0.2f, 0.3f)), FMargin(22.f, 0.f));
	Footer->SetVerticalAlignment(VAlign_Center);
	AddV(Column, Sized(WidgetTree, TEXT("FooterSize"), Footer, 0.f, 64.f), FMargin(0.f, 18.f, 0.f, 0.f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FooterRow"));
	Footer->SetContent(Row);
	FooterHints = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FooterHints"));
	AddH(Row, FooterHints, FMargin(0.f), VAlign_Center, 1.f);

	// Kept for compatibility with page data; the left text is replaced by keycaps.
	FooterLeft = MakeText(WidgetTree, TEXT("FooterLeft"), RuinmatesUI::Font(15), RuinmatesUI::TextWeak());
	FooterLeft->SetVisibility(ESlateVisibility::Collapsed);
	AddH(Row, FooterLeft);
	AddH(Row, MakeImage(WidgetTree, TEXT("FooterDot"), RuinmatesUI::Image(TEXT("Slots"), TEXT("T_UI_RarityPip")), FVector2D(10.f, 10.f), RuinmatesUI::Success()),
		FMargin(0.f, 0.f, 10.f, 0.f));
	FooterRight = MakeText(WidgetTree, TEXT("FooterRight"), RuinmatesUI::Font(17), RuinmatesUI::TextSecondary(), NSLOCTEXT("RuinMatesUI", "NotPaused", "游戏不暂停"));
	AddH(Row, FooterRight);
	RebuildFooterHints(false);
}

void URuinmatesTerminalScreen::RebuildFooterHints(bool bSearch)
{
	const int32 Mode = bSearch ? 1 : 0;
	if (!FooterHints || FooterMode == Mode)
	{
		return;
	}
	FooterMode = Mode;
	FooterHints->ClearChildren();

	struct FHint
	{
		FText Key;
		FText Label;
	};
	TArray<FHint> Hints;
	if (bSearch)
	{
		Hints = {
			{ NSLOCTEXT("RuinMatesUI", "KeyEsc", "ESC"), NSLOCTEXT("RuinMatesUI", "HintClose", "关闭") },
			{ NSLOCTEXT("RuinMatesUI", "KeyLmb", "左键"), NSLOCTEXT("RuinMatesUI", "HintPickup", "拾取已发现物品") },
			{ NSLOCTEXT("RuinMatesUI", "KeyRmb", "右键"), NSLOCTEXT("RuinMatesUI", "HintDiscard", "丢弃背包物品") },
		};
	}
	else
	{
		Hints = {
			{ NSLOCTEXT("RuinMatesUI", "KeyTabEsc", "TAB / ESC"), NSLOCTEXT("RuinMatesUI", "HintClose", "关闭") },
			{ NSLOCTEXT("RuinMatesUI", "KeyLmb", "左键"), NSLOCTEXT("RuinMatesUI", "HintSelect", "选择") },
			{ NSLOCTEXT("RuinMatesUI", "KeyRmb", "右键"), NSLOCTEXT("RuinMatesUI", "HintDiscard", "丢弃背包物品") },
		};
	}
	for (int32 Index = 0; Index < Hints.Num(); ++Index)
	{
		UBorder* Cap = MakeBrushBorder(WidgetTree, NAME_None, RuinmatesUI::KeycapBrush(), FMargin(12.f, 4.f, 12.f, 7.f));
		Cap->SetContent(MakeText(WidgetTree, NAME_None, RuinmatesUI::FontBold(15), RuinmatesUI::TextPrimary(), Hints[Index].Key));
		Cap->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddH(FooterHints, Cap, FMargin(Index == 0 ? 0.f : 28.f, 0.f, 10.f, 0.f));
		AddH(FooterHints, MakeText(WidgetTree, NAME_None, RuinmatesUI::Font(17), RuinmatesUI::TextSecondary(), Hints[Index].Label));
	}
}

void URuinmatesTerminalScreen::BuildMenuLayer(UCanvasPanel* Canvas)
{
	// Slot action menu lives above everything else so it is never clipped by a panel.
	MenuLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MenuLayer"));
	MenuLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* LayerSlot = Canvas->AddChildToCanvas(MenuLayer))
	{
		LayerSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		LayerSlot->SetOffsets(FMargin(0.f));
	}

	// Transparent full-screen catcher: a click outside the menu closes it.
	MenuDismissButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MenuDismiss"));
	{
		FButtonStyle DismissStyle;
		DismissStyle.Normal = FSlateColorBrush(FLinearColor::Transparent);
		DismissStyle.Hovered = FSlateColorBrush(FLinearColor::Transparent);
		DismissStyle.Pressed = FSlateColorBrush(FLinearColor::Transparent);
		DismissStyle.NormalPadding = FMargin(0.f);
		DismissStyle.PressedPadding = FMargin(0.f);
		MenuDismissButton->SetStyle(DismissStyle);
	}
	MenuDismissButton->SetVisibility(ESlateVisibility::Collapsed);
	MenuDismissButton->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleMenuDismiss);
	if (UCanvasPanelSlot* DismissSlot = MenuLayer->AddChildToCanvas(MenuDismissButton))
	{
		DismissSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DismissSlot->SetOffsets(FMargin(0.f));
	}

	// Menu: chamfered panel with the item name and a discard action.
	UBorder* MenuPanel = MakeBrushBorder(WidgetTree, TEXT("ContextMenu"), RuinmatesUI::PanelBrush(), FMargin(18.f, 16.f));
	MenuPanel->SetVisibility(ESlateVisibility::Collapsed);
	ContextMenu = MenuPanel;
	if (UCanvasPanelSlot* MenuSlot = MenuLayer->AddChildToCanvas(MenuPanel))
	{
		MenuSlot->SetAutoSize(true);
		MenuSlot->SetPosition(FVector2D(240.f, 240.f));
	}

	UVerticalBox* MenuColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuColumn"));
	MenuPanel->SetContent(MenuColumn);
	MenuTitle = MakeText(WidgetTree, TEXT("MenuTitle"), RuinmatesUI::FontBold(18), RuinmatesUI::TextPrimary());
	AddV(MenuColumn, MenuTitle, FMargin(0.f, 0.f, 0.f, 12.f));

	UTextBlock* DiscardLabel = MakeText(WidgetTree, TEXT("MenuDiscardLabel"), RuinmatesUI::FontBold(18), RuinmatesUI::Error(),
		NSLOCTEXT("RuinMatesUI", "Discard", "丢弃"));
	UButton* DiscardButton = MakeStyledButton(WidgetTree, TEXT("MenuDiscard"), RuinmatesUI::CloseButtonStyle(), DiscardLabel, FMargin(0.f));
	DiscardButton->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleMenuDiscard);
	AddV(MenuColumn, Sized(WidgetTree, TEXT("MenuDiscardSize"), DiscardButton, 180.f, 48.f));

	// Sort lives in the menu too: one entry point for whole-backpack actions.
	UTextBlock* SortLabel = MakeText(WidgetTree, TEXT("MenuSortLabel"), RuinmatesUI::FontBold(18), RuinmatesUI::TextPrimary(),
		NSLOCTEXT("RuinMatesUI", "SortBackpack", "整理背包"));
	UButton* SortEntry = MakeStyledButton(WidgetTree, TEXT("MenuSort"), RuinmatesUI::ButtonStyle(), SortLabel, FMargin(0.f));
	SortEntry->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleSortBackpack);
	AddV(MenuColumn, Sized(WidgetTree, TEXT("MenuSortSize"), SortEntry, 180.f, 44.f));

	// Split controls: hidden until the slot menu asks for them.
	UTextBlock* SplitLabel = MakeText(WidgetTree, TEXT("MenuSplitLabel"), RuinmatesUI::FontBold(18), RuinmatesUI::TextPrimary(),
		NSLOCTEXT("RuinMatesUI", "SplitStack", "拆分"));
	UButton* SplitEntry = MakeStyledButton(WidgetTree, TEXT("MenuSplit"), RuinmatesUI::ButtonStyle(), SplitLabel, FMargin(0.f));
	SplitEntry->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleMenuSplit);
	AddV(MenuColumn, Sized(WidgetTree, TEXT("MenuSplitSize"), SplitEntry, 180.f, 44.f));

	SplitPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SplitPanel"));
	SplitPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddV(MenuColumn, SplitPanel);
	SplitHintText = MakeText(WidgetTree, TEXT("SplitHint"), RuinmatesUI::Font(14), RuinmatesUI::TextWeak(),
		NSLOCTEXT("RuinMatesUI", "SplitHint", "选择拆分数量"));
	AddV(SplitPanel, SplitHintText, FMargin(0.f, 4.f, 0.f, 4.f));
	SplitSlider = CreateWidget<URuinmatesTechSlider>(this);
	if (SplitSlider)
	{
		SplitSlider->OnValueChanged.AddDynamic(this, &URuinmatesTerminalScreen::HandleSplitValueChanged);
		AddV(SplitPanel, Sized(WidgetTree, TEXT("SplitSliderSize"), SplitSlider, 180.f, 32.f));
	}
	SplitCountText = MakeText(WidgetTree, TEXT("SplitCount"), RuinmatesUI::FontBold(18), RuinmatesUI::TextPrimary(), FText::GetEmpty());
	AddV(SplitPanel, SplitCountText, FMargin(0.f, 2.f, 0.f, 6.f));
	UTextBlock* SplitConfirmLabel = MakeText(WidgetTree, TEXT("SplitConfirmLabel"), RuinmatesUI::FontBold(16), RuinmatesUI::TextPrimary(),
		NSLOCTEXT("RuinMatesUI", "SplitConfirm", "确认拆分"));
	UButton* SplitConfirm = MakeStyledButton(WidgetTree, TEXT("SplitConfirm"), RuinmatesUI::ButtonStyle(), SplitConfirmLabel, FMargin(0.f));
	SplitConfirm->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleSplitConfirm);
	AddV(SplitPanel, Sized(WidgetTree, TEXT("SplitConfirmSize"), SplitConfirm, 180.f, 44.f));
	UTextBlock* SplitCancelLabel = MakeText(WidgetTree, TEXT("SplitCancelLabel"), RuinmatesUI::FontBold(16), RuinmatesUI::TextWeak(),
		NSLOCTEXT("RuinMatesUI", "SplitCancel", "取消"));
	UButton* SplitCancel = MakeStyledButton(WidgetTree, TEXT("SplitCancel"), RuinmatesUI::CloseButtonStyle(), SplitCancelLabel, FMargin(0.f));
	SplitCancel->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleSplitCancel);
	AddV(SplitPanel, Sized(WidgetTree, TEXT("SplitCancelSize"), SplitCancel, 180.f, 40.f));
}

void URuinmatesTerminalScreen::OpenFor(ARuinmatesCharacter* InCharacter, ARuinmatesLootContainer* InContainer)
{
	CloseSlotMenu();
	bLive = true;
	bMarketLive = false;
	LiveMerchant = nullptr;
	bClosing = false;
	VisualMode = IsValid(InContainer) ? 1 : 0;
	LiveCharacter = InCharacter;
	LiveContainer = InContainer;
	ActiveFilter = NAME_None;
	BindSources();
	RefreshFromLive();
	if (PreviewNavigation) PreviewNavigation->SetVisibility(ESlateVisibility::Collapsed);
	if (PreviewBadge) PreviewBadge->SetVisibility(ESlateVisibility::Collapsed);
	if (CharacterPreview) CharacterPreview->SetCharacter(VisualMode == 0 ? InCharacter : nullptr);
	if (SquadRadio) SquadRadio->SetPreview(false);
	BeginOpenFade();
}

void URuinmatesTerminalScreen::ShowPreview(bool bSearchPage)
{
	if (DesignCanvas)
	{
		ShowVisualPreview(bSearchPage ? 1 : 0);
		return;
	}
	CloseSlotMenu();
	UnbindSources();
	bLive = false;
	bClosing = false;
	LiveCharacter = nullptr;
	LiveContainer = nullptr;
	Page = bSearchPage ? RuinmatesTerminalPreview::MakeSearchPage() : RuinmatesTerminalPreview::MakeInventoryPage();
	ActiveFilter = NAME_None;
	SelectedStableIndex = INDEX_NONE;
	ApplyPageText();
	RebuildSlots();
	BeginOpenFade();
}

void URuinmatesTerminalScreen::RefreshTabStyles()
{
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		const bool bSelected = TabFilters.IsValidIndex(Index) && TabFilters[Index] == ActiveFilter;
		if (UButton* Tab = TabButtons[Index])
		{
			if (URuinmatesTechButton* Tech = Cast<URuinmatesTechButton>(Tab))
			{
				Tech->SetSelected(bSelected);
			}
			else
			{
				Tab->SetStyle(RuinmatesUI::TabStyle(bSelected));
			}
		}
		const FLinearColor Ink = bSelected ? FLinearColor::White : RuinmatesUI::TextSecondary();
		if (UTextBlock* Label = TabLabels.IsValidIndex(Index) ? TabLabels[Index].Get() : nullptr)
		{
			Label->SetColorAndOpacity(FSlateColor(Ink));
		}
		if (UImage* Icon = TabIcons.IsValidIndex(Index) ? TabIcons[Index].Get() : nullptr)
		{
			Icon->SetColorAndOpacity(bSelected ? FLinearColor::White : RuinmatesUI::WithAlpha(RuinmatesUI::TextSecondary(), 0.85f));
		}
	}
}

void URuinmatesTerminalScreen::ApplyPageText()
{
	if (PageTitle)
	{
		PageTitle->SetText(Page.PageTitle);
	}
	if (ClockText)
	{
		const FString Clock = Page.StatusText.IsEmpty()
			? FDateTime::Now().ToString(TEXT("%H:%M"))
			: Page.StatusText.ToString();
		ClockText->SetText(FText::FromString(Clock));
	}
	if (PackTitle)
	{
		PackTitle->SetText(Page.PackTitle);
	}
	const float Ratio = Page.Capacity > 0 ? static_cast<float>(Page.Used) / static_cast<float>(Page.Capacity) : 0.f;
	const bool bFull = Ratio >= 1.f;
	if (CapacityUsedText)
	{
		CapacityUsedText->SetText(FText::AsNumber(Page.Used));
		CapacityUsedText->SetColorAndOpacity(FSlateColor(bFull ? RuinmatesUI::Warning() : RuinmatesUI::TextPrimary()));
	}
	if (CapacityText)
	{
		CapacityText->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "CapacityMax", " / {0}"), FText::AsNumber(Page.Capacity)));
	}
	if (CapacityBar)
	{
		CapacityBar->SetPercent(Ratio);
		CapacityBar->SetFillColorAndOpacity(bFull ? RuinmatesUI::Warning() : RuinmatesUI::Cyan());
	}
	if (ValueText)
	{
		ValueText->SetText(FText::AsNumber(Page.LootValue));
	}
	if (FundsChip)
	{
		FundsChip->SetVisibility(Page.Funds >= 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (FundsText && Page.Funds >= 0)
	{
		FundsText->SetText(FText::AsNumber(Page.Funds));
	}
	if (RightSwitcher)
	{
		RightSwitcher->SetActiveWidgetIndex(DesignCanvas && VisualMode >= 3 ? VisualMode - 1 : Page.bRightIsContainer ? 1 : 0);
	}
	if (!Page.bRightIsContainer)
	{
		if (RightTitle)
		{
			RightTitle->SetText(Page.RightTitle);
		}
		if (RightSubtitle)
		{
			RightSubtitle->SetText(Page.RightSubtitle.IsEmpty() ? FText::FromString(TEXT("LV.1")) : Page.RightSubtitle);
		}
		if (EquipBackpackText)
		{
			EquipBackpackText->SetText(Page.BackpackTierName.IsEmpty() ? Page.PackTitle : Page.BackpackTierName);
			EquipBackpackText->SetColorAndOpacity(FSlateColor(RuinmatesUI::Cyan()));
		}
		if (EquipPrimaryText)
		{
			EquipPrimaryText->SetText(Page.bHasWeapon
				? NSLOCTEXT("RuinMatesUI", "EquipHasWeapon", "已装备")
				: NSLOCTEXT("RuinMatesUI", "EquipNoWeapon", "空"));
			EquipPrimaryText->SetColorAndOpacity(FSlateColor(Page.bHasWeapon ? RuinmatesUI::Cyan() : RuinmatesUI::TextWeak()));
		}
	}
	else
	{
		int32 Revealed = 0;
		int32 Hidden = 0;
		for (const FRuinmatesTerminalSlotView& View : Page.RightSlots)
		{
			Revealed += View.State == ERuinmatesTerminalSlotState::Revealed ? 1 : 0;
			Hidden += (View.State == ERuinmatesTerminalSlotState::Unknown || View.State == ERuinmatesTerminalSlotState::Searching) ? 1 : 0;
		}
		if (LootTitleText)
		{
			LootTitleText->SetText(Page.RightTitle);
		}
		if (LootCountText)
		{
			LootCountText->SetText(Page.RightSubtitle);
		}
		if (SearchBar)
		{
			SearchBar->SetPercent(Page.RightProgress);
		}
		if (SearchStatusText)
		{
			SearchStatusText->SetText(Hidden > 0
				? NSLOCTEXT("RuinMatesUI", "SearchRunning", "正在搜索剩余物品…")
				: NSLOCTEXT("RuinMatesUI", "SearchDone", "搜索完成"));
			SearchStatusText->SetColorAndOpacity(FSlateColor(Hidden > 0 ? RuinmatesUI::Cyan() : RuinmatesUI::Success()));
		}
		if (TakeAllButton)
		{
			TakeAllButton->SetIsEnabled(Revealed > 0);
		}
		if (TakeAllLabel)
		{
			TakeAllLabel->SetColorAndOpacity(FSlateColor(Revealed > 0 ? RuinmatesUI::TextPrimary() : RuinmatesUI::TextWeak()));
		}
	}
	if (FooterLeft)
	{
		FooterLeft->SetText(Page.FooterLeft);
	}
	if (FooterRight)
	{
		FooterRight->SetText(Page.FooterRight.IsEmpty() ? NSLOCTEXT("RuinMatesUI", "NotPaused", "游戏不暂停") : Page.FooterRight);
	}
	RebuildFooterHints(Page.bRightIsContainer);
	RefreshTabStyles();
	RefreshVisualPage();
}

void URuinmatesTerminalScreen::SyncGrid(UUniformGridPanel* Grid, TArray<TObjectPtr<URuinmatesTerminalItemSlot>>& Cells,
	const TArray<FRuinmatesTerminalSlotView>& Views, int32 Columns, bool bLoot)
{
	if (!Grid)
	{
		return;
	}
	// Shrink: drop surplus cells from the end.
	while (Cells.Num() > Views.Num())
	{
		if (URuinmatesTerminalItemSlot* Last = Cells.Pop())
		{
			Last->RemoveFromParent();
		}
	}
	// Grow: create only the missing cells. Existing ones keep hover / animation state.
	const bool bGrew = Cells.Num() < Views.Num();
	while (Cells.Num() < Views.Num())
	{
		URuinmatesTerminalItemSlot* Cell = CreateWidget<URuinmatesTerminalItemSlot>(this, URuinmatesTerminalItemSlot::StaticClass());
		if (!Cell)
		{
			break;
		}
		const int32 Index = Cells.Num();
		if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Cell, Index / Columns, Index % Columns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (bLoot)
		{
			Cell->OnSlotClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleLootClicked);
		}
		else
		{
			Cell->OnSlotClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleSlotClicked);
			Cell->OnSlotRightClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleSlotRightClicked);
		}
		Cells.Add(Cell);
	}
	for (int32 Index = 0; Index < Cells.Num() && Index < Views.Num(); ++Index)
	{
		if (URuinmatesTerminalItemSlot* Cell = Cells[Index])
		{
			Cell->ApplyView(Views[Index]);
			if (!bLoot)
			{
				Cell->SetBackpackDragEnabled(bLive && ActiveFilter.IsNone());
			}
		}
	}
	if (bGrew)
	{
		UpdateCellSizes(true);
	}
}

void URuinmatesTerminalScreen::RebuildSlots()
{
	TArray<FRuinmatesTerminalSlotView> Shown = RuinmatesTerminalDisplay::FilterBackpackSlots(Page.Backpack, ActiveFilter);
	SyncGrid(BackpackGrid, BackpackSlots, Shown, BackpackColumns, false);
	if (CategoryEmptyHint)
	{
		CategoryEmptyHint->SetVisibility(!ActiveFilter.IsNone() && Shown.IsEmpty()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Page.bRightIsContainer)
	{
		SyncGrid(LootGrid, LootSlots, Page.RightSlots, LootColumns, true);
	}
	else if (LootSlots.Num() > 0)
	{
		SyncGrid(LootGrid, LootSlots, TArray<FRuinmatesTerminalSlotView>(), LootColumns, true);
	}
}

void URuinmatesTerminalScreen::SetFilter(FName InFilter)
{
	if (ActiveFilter == InFilter)
	{
		return;
	}
	ActiveFilter = InFilter;
	FilterFadeTime = 0.f;
	if (BackpackGrid)
	{
		BackpackGrid->SetRenderOpacity(0.76f);
	}
	RefreshTabStyles();
	RebuildSlots();
}

void URuinmatesTerminalScreen::SelectAll() { SetFilter(NAME_None); }
void URuinmatesTerminalScreen::SelectConsumable() { SetFilter(TEXT("Consumable")); }
void URuinmatesTerminalScreen::SelectMaterial() { SetFilter(TEXT("Material")); }
void URuinmatesTerminalScreen::SelectQuest() { SetFilter(TEXT("Quest")); }
void URuinmatesTerminalScreen::SelectAmmo() { SetFilter(TEXT("Ammo")); }
void URuinmatesTerminalScreen::SelectValuable() { SetFilter(TEXT("Valuable")); }
void URuinmatesTerminalScreen::SelectWeapon() { SetFilter(TEXT("Weapon")); }
void URuinmatesTerminalScreen::SelectEquipment() { SetFilter(TEXT("Equipment")); }

void URuinmatesTerminalScreen::HandleSlotClicked(int32 StableIndex)
{
	CloseSlotMenu();
	SelectedStableIndex = StableIndex;
	for (FRuinmatesTerminalSlotView& View : Page.Backpack)
	{
		View.bSelected = View.StableIndex == StableIndex && View.State == ERuinmatesTerminalSlotState::Revealed;
	}
	RebuildSlots();
	if (bMarketLive)
	{
		// The live market owns selection: a click on the left backpack picks a sell stack.
		if (bVisualSell)
		{
			HandleMarketSelected(StableIndex);
		}
		return;
	}
	if (!bLive && VisualMode == 3 && bVisualSell)
	{
		VisualSelected = StableIndex;
		VisualQuantity = 1;
		RefreshVisualPage();
	}
}

void URuinmatesTerminalScreen::HandleSlotRightClicked(int32 StableIndex)
{
	if (!bLive)
	{
		return;
	}
	OpenSlotMenu(StableIndex);
}

void URuinmatesTerminalScreen::HandleSortBackpack()
{
	CloseSlotMenu();
	ARuinmatesCharacter* Character = LiveCharacter.Get();
	URuinmatesBackpackComponent* Backpack = Character ? Character->GetBackpack() : nullptr;
	if (!bLive || !Backpack)
	{
		return;
	}
	// Server merges, orders and pushes gaps to the end; the revision rejects stale requests.
	Character->RequestSortBackpack(Backpack->GetRevision());
}

void URuinmatesTerminalScreen::HandleMenuSplit()
{
	ARuinmatesCharacter* Character = LiveCharacter.Get();
	const FRuinmatesTerminalSlotView* Match = Page.Backpack.FindByPredicate(
		[this](const FRuinmatesTerminalSlotView& View)
		{
			return View.StableIndex == MenuStableIndex && View.State == ERuinmatesTerminalSlotState::Revealed;
		});
	if (!Character || !Match || !SplitPanel)
	{
		return;
	}

	SplitPanel->SetVisibility(ESlateVisibility::Visible);
	if (Match->Count <= 1)
	{
		SplitMaxCount = 0;
		if (SplitHintText) SplitHintText->SetText(NSLOCTEXT("RuinMatesUI", "SplitSingle", "数量为 1 的物品无法拆分"));
		if (SplitSlider) SplitSlider->SetVisibility(ESlateVisibility::Collapsed);
		if (SplitCountText) SplitCountText->SetText(FText::GetEmpty());
		return;
	}

	SplitMaxCount = Match->Count - 1;
	if (SplitSlider)
	{
		SplitSlider->SetVisibility(ESlateVisibility::Visible);
		SplitSlider->SetValueRange(1.f, static_cast<float>(SplitMaxCount), 1.f);
	}
	if (SplitHintText)
	{
		SplitHintText->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "SplitRange", "拆分数量 1 - {0}"),
			FText::AsNumber(SplitMaxCount)));
	}
	if (SplitCountText) SplitCountText->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "SplitCountFmt", "拆分 {0}"),
		FText::AsNumber(1)));
}

void URuinmatesTerminalScreen::HandleSplitValueChanged(float Value)
{
	if (!SplitCountText || SplitMaxCount <= 0)
	{
		return;
	}
	const int32 Quantity = FMath::Clamp(FMath::RoundToInt(Value), 1, SplitMaxCount);
	SplitCountText->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "SplitCountFmt", "拆分 {0}"), FText::AsNumber(Quantity)));
}

void URuinmatesTerminalScreen::HandleSplitConfirm()
{
	ARuinmatesCharacter* Character = LiveCharacter.Get();
	URuinmatesBackpackComponent* Backpack = Character ? Character->GetBackpack() : nullptr;
	if (!Character || !Backpack || MenuStableIndex == INDEX_NONE || SplitMaxCount <= 0)
	{
		return;
	}

	const int32 Quantity = FMath::Clamp(
		FMath::RoundToInt(SplitSlider ? SplitSlider->GetValue() : 1.f), 1, FMath::Max(1, SplitMaxCount));
	const bool bHasEmptySlot = Backpack->GetSlots().ContainsByPredicate(
		[](const FRuinmatesItemStack& Stack) { return Stack.IsEmpty(); });
	if (!bHasEmptySlot)
	{
		if (SplitHintText)
		{
			SplitHintText->SetText(NSLOCTEXT("RuinMatesUI", "SplitNoRoom", "背包已满，无法拆分"));
		}
		return;
	}

	Character->RequestSplitStack(MenuStableIndex, Quantity, Backpack->GetRevision());
	CloseSlotMenu();
}

void URuinmatesTerminalScreen::HandleSplitCancel()
{
	// Cancel never touches the backpack.
	CloseSlotMenu();
}

void URuinmatesTerminalScreen::OpenSlotMenu(int32 StableIndex)
{
	if (!ContextMenu || !MenuDismissButton || !MenuTitle)
	{
		return;
	}

	const FRuinmatesTerminalSlotView* Match = Page.Backpack.FindByPredicate(
		[StableIndex](const FRuinmatesTerminalSlotView& View)
		{
			return View.StableIndex == StableIndex && View.State == ERuinmatesTerminalSlotState::Revealed;
		});
	if (!Match)
	{
		CloseSlotMenu();
		return;
	}

	MenuStableIndex = StableIndex;
	MenuTitle->SetText(Match->Name);
	if (SplitPanel) SplitPanel->SetVisibility(ESlateVisibility::Collapsed);
	SplitMaxCount = 0;
	MenuDismissButton->SetVisibility(ESlateVisibility::Visible);
	ContextMenu->SetVisibility(ESlateVisibility::Visible);

	// Place the menu at the cursor, kept inside the viewport.
	FVector2D MenuPosition(240.f, 240.f);
	const APlayerController* Player = GetOwningPlayer();
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (Player && UWidgetLayoutLibrary::GetMousePositionScaledByDPI(Player, MouseX, MouseY))
	{
		const FVector2D ViewSize = UWidgetLayoutLibrary::GetViewportSize(this);
		const float MaxX = FMath::Max(8.f, ViewSize.X - 300.f);
		const float MaxY = FMath::Max(8.f, ViewSize.Y - 170.f);
		MenuPosition = FVector2D(
			FMath::Clamp(MouseX, 8.f, MaxX),
			FMath::Clamp(MouseY, 8.f, MaxY));
	}
	if (UCanvasPanelSlot* MenuSlot = Cast<UCanvasPanelSlot>(ContextMenu->Slot))
	{
		MenuSlot->SetAutoSize(true);
		MenuSlot->SetPosition(MenuPosition);
	}
}

void URuinmatesTerminalScreen::CloseSlotMenu()
{
	MenuStableIndex = INDEX_NONE;
	if (ContextMenu)
	{
		ContextMenu->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MenuDismissButton)
	{
		MenuDismissButton->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URuinmatesTerminalScreen::HandleMenuDismiss()
{
	CloseSlotMenu();
}

void URuinmatesTerminalScreen::HandleMenuDiscard()
{
	const int32 SlotIndex = MenuStableIndex;
	CloseSlotMenu();
	if (SlotIndex == INDEX_NONE || !bLive)
	{
		return;
	}
	// Only the server removes the stack; the UI just asks and waits for the replicated refresh.
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		Character->RequestDiscard(SlotIndex);
	}
}

void URuinmatesTerminalScreen::HandleLootClicked(int32 StableIndex)
{
	if (!bLive)
	{
		return;
	}
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		Character->RequestPickup(StableIndex);
	}
}

void URuinmatesTerminalScreen::HandleTakeAll()
{
	if (!bLive)
	{
		return;
	}
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		Character->RequestTakeAllLoot();
	}
}

void URuinmatesTerminalScreen::BindSources()
{
	UnbindSources();
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		if (URuinmatesBackpackComponent* Backpack = Character->GetBackpack())
		{
			Backpack->OnChanged.AddUObject(this, &URuinmatesTerminalScreen::Refresh);
		}
	}
	if (ARuinmatesLootContainer* Container = LiveContainer.Get())
	{
		Container->OnChanged.AddUObject(this, &URuinmatesTerminalScreen::Refresh);
	}
	// Team wallet: read-only display of the replicated server balance.
	if (UWorld* World = GetWorld())
	{
		if (ARuinmatesGameState* GameState = World->GetGameState<ARuinmatesGameState>())
		{
			LiveGameState = GameState;
			GameState->OnSharedFundsChanged.AddUObject(this, &URuinmatesTerminalScreen::Refresh);
		}
	}
}

void URuinmatesTerminalScreen::UnbindSources()
{
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		if (URuinmatesBackpackComponent* Backpack = Character->GetBackpack())
		{
			Backpack->OnChanged.RemoveAll(this);
		}
	}
	if (ARuinmatesLootContainer* Container = LiveContainer.Get())
	{
		Container->OnChanged.RemoveAll(this);
	}
	if (ARuinmatesGameState* GameState = LiveGameState.Get())
	{
		GameState->OnSharedFundsChanged.RemoveAll(this);
	}
	LiveGameState = nullptr;
}

void URuinmatesTerminalScreen::Refresh()
{
	// One refresh entry for every live binding (backpack OnChanged, container OnChanged, wallet).
	if (bMarketLive)
	{
		RefreshMarketLive();
	}
	else if (bLive)
	{
		RefreshFromLive();
	}
}

void URuinmatesTerminalScreen::MakeSlotView(const FRuinmatesItemStack& Stack, int32 StableIndex, FRuinmatesTerminalSlotView& OutView) const
{
	OutView = FRuinmatesTerminalSlotView();
	OutView.StableIndex = StableIndex;
	if (Stack.IsEmpty())
	{
		OutView.State = ERuinmatesTerminalSlotState::Empty;
		return;
	}
	OutView.State = ERuinmatesTerminalSlotState::Revealed;
	OutView.ItemId = Stack.ItemId;
	OutView.Name = Stack.DisplayName;
	OutView.Count = Stack.Count;
	OutView.Quality = Stack.Quality;
	OutView.Description = Stack.Description;
	OutView.UnitValue = Stack.UnitValue;
	FillFromDatabase(this, Stack, OutView);
}

void URuinmatesTerminalScreen::RefreshFromLive()
{
	ARuinmatesCharacter* Character = LiveCharacter.Get();
	if (!Character)
	{
		return;
	}

	URuinmatesBackpackComponent* Backpack = Character->GetBackpack();
	ARuinmatesLootContainer* Container = LiveContainer.Get();

	FRuinmatesTerminalPreviewPage Next;
	Next.PageTitle = Container
		? NSLOCTEXT("RuinMatesUI", "PageSearch", "物资回收")
		: NSLOCTEXT("RuinMatesUI", "PageInventory", "背包");
	Next.PackTitle = Backpack
		? RuinmatesItems::GetBackpackTierName(Backpack->GetTier())
		: NSLOCTEXT("RuinMatesUI", "PackMissing", "背包");
	Next.Capacity = Backpack ? Backpack->GetCapacity() : 0;
	Next.Used = Backpack ? Backpack->GetUsedSlotCount() : 0;
	Next.LootValue = Backpack ? Backpack->GetLootValue() : 0;
	Next.BackpackTierName = Next.PackTitle;
	Next.bHasWeapon = Character->GetEquippedWeapon() != nullptr;
	if (const ARuinmatesGameState* GameState = LiveGameState.Get())
	{
		Next.Funds = GameState->GetSharedFunds();
	}

	const TArray<FRuinmatesItemStack> NoSlots;
	const TArray<FRuinmatesItemStack>& Slots = Backpack ? Backpack->GetSlots() : NoSlots;
	const int32 SlotCount = FMath::Max(Next.Capacity, Slots.Num());
	Next.Backpack.Reserve(SlotCount);
	bool bSelectionStillValid = false;
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		FRuinmatesTerminalSlotView View;
		View.StableIndex = Index;
		if (Slots.IsValidIndex(Index) && !Slots[Index].IsEmpty())
		{
			const FRuinmatesItemStack& Stack = Slots[Index];
			View.State = ERuinmatesTerminalSlotState::Revealed;
			View.ItemId = Stack.ItemId;
			View.Name = Stack.DisplayName;
			View.Count = Stack.Count;
			View.Quality = Stack.Quality;
			FillFromDatabase(this, Stack, View);
			View.bSelected = Index == SelectedStableIndex;
			bSelectionStillValid |= View.bSelected;
		}
		else
		{
			View.State = ERuinmatesTerminalSlotState::Empty;
		}
		Next.Backpack.Add(View);
	}
	if (!bSelectionStillValid)
	{
		SelectedStableIndex = INDEX_NONE;
	}

	if (Container)
	{
		Next.bRightIsContainer = true;
		Next.RightTitle = Container->GetContainerName();
		const TArray<FRuinmatesItemStack>& Contents = Container->GetContents();
		int32 Revealed = 0;
		int32 SearchingIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Contents.Num(); ++Index)
		{
			if (Contents[Index].bRevealed)
			{
				++Revealed;
			}
			else if (SearchingIndex == INDEX_NONE)
			{
				SearchingIndex = Index;
			}
		}
		Next.RightSubtitle = FText::Format(
			NSLOCTEXT("RuinMatesUI", "LiveSearchCount", "{0} / {1}"),
			FText::AsNumber(Revealed),
			FText::AsNumber(Contents.Num()));
		Next.RightProgress = Contents.Num() > 0 ? static_cast<float>(Revealed) / static_cast<float>(Contents.Num()) : 1.f;
		Next.RightSlots.Reserve(Contents.Num());
		for (int32 Index = 0; Index < Contents.Num(); ++Index)
		{
			FRuinmatesTerminalSlotView View;
			View.StableIndex = Index;
			const FRuinmatesItemStack& Stack = Contents[Index];
			if (Stack.IsEmpty())
			{
				View.State = Stack.bRevealed
					? ERuinmatesTerminalSlotState::Taken
					: Index == SearchingIndex ? ERuinmatesTerminalSlotState::Searching : ERuinmatesTerminalSlotState::Unknown;
			}
			else if (!Stack.bRevealed)
			{
				View.State = Index == SearchingIndex
					? ERuinmatesTerminalSlotState::Searching
					: ERuinmatesTerminalSlotState::Unknown;
			}
			else
			{
				View.State = ERuinmatesTerminalSlotState::Revealed;
				View.ItemId = Stack.ItemId;
				View.Name = Stack.DisplayName;
				View.Count = Stack.Count;
				View.Quality = Stack.Quality;
				FillFromDatabase(this, Stack, View);
			}
			Next.RightSlots.Add(View);
		}
		Next.FooterLeft = NSLOCTEXT("RuinMatesUI", "LiveSearchFooter", "ESC 关闭    点击已发现物品拾取    右键丢弃");
		Next.FooterRight = NSLOCTEXT("RuinMatesUI", "NotPaused", "游戏不暂停");
	}
	else
	{
		FText SurvivorName = NSLOCTEXT("RuinMatesUI", "Survivor", "幸存者");
		if (APlayerState* PlayerState = Character->GetPlayerState())
		{
			if (!PlayerState->GetPlayerName().IsEmpty())
			{
				SurvivorName = FText::FromString(PlayerState->GetPlayerName());
			}
		}
		Next.RightTitle = SurvivorName;
		if (ARuinmatesPlayerState* RuinState = Character->GetPlayerState<ARuinmatesPlayerState>())
		{
			Next.RightSubtitle = FText::Format(
				NSLOCTEXT("RuinMatesUI", "LiveLevel", "LV.{0}"),
				FText::AsNumber(RuinState->GetCampaignLevel()));
		}
		Next.FooterLeft = NSLOCTEXT("RuinMatesUI", "LiveInvFooter", "TAB 关闭    右键丢弃");
	}

	Page = Next;
	ApplyPageText();
	RebuildSlots();
}

void URuinmatesTerminalScreen::ClosePreview()
{
	if (bClosing)
	{
		return;
	}
	bClosing = true;
	if (bMarketLive)
	{
		// Leaving the market ends the trade so the server stops accepting requests from this UI.
		if (ARuinmatesCharacter* Character = LiveCharacter.Get())
		{
			Character->EndTrade();
			return;
		}
	}
	if (bLive)
	{
		if (ARuinmatesCharacter* Character = LiveCharacter.Get())
		{
			Character->CloseTerminal();
			return;
		}
	}
	RemoveFromParent();
	if (!GEngine)
	{
		return;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game))
		{
			continue;
		}
		if (APlayerController* Player = World->GetFirstPlayerController())
		{
			Player->SetShowMouseCursor(false);
			Player->SetInputMode(FInputModeGameOnly());
		}
	}
}

static void OpenTerminalPreview(bool bSearch)
{
	if (!GEngine)
	{
		return;
	}
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World() && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game))
		{
			World = Context.World();
			break;
		}
	}
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("Ruinmates terminal preview needs a running PIE or game world."));
		return;
	}
	APlayerController* Player = World->GetFirstPlayerController();
	if (!Player)
	{
		return;
	}
	TArray<UUserWidget*> Existing;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Existing, URuinmatesTerminalScreen::StaticClass(), true);
	for (UUserWidget* Widget : Existing) Widget->RemoveFromParent();
	URuinmatesTerminalScreen* Screen = CreateWidget<URuinmatesTerminalScreen>(Player, URuinmatesTerminalScreen::StaticClass());
	if (!Screen)
	{
		return;
	}
	Screen->AddToViewport(40);
	Screen->ShowPreview(bSearch);
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(Screen->TakeWidget());
	Player->SetInputMode(InputMode);
	Player->SetShowMouseCursor(true);
}

static void OpenVisualTerminal(int32 Mode, int32 PreviewCapacity = 20)
{
	if (!GEngine) return;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || (Context.WorldType != EWorldType::PIE && Context.WorldType != EWorldType::Game)) continue;
		APlayerController* Player = World->GetFirstPlayerController();
		if (!Player || !Player->IsLocalController()) continue;
		TArray<UUserWidget*> Existing;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Existing, URuinmatesTerminalScreen::StaticClass(), true);
		for (UUserWidget* Widget : Existing) Widget->RemoveFromParent();
		URuinmatesTerminalScreen* Screen = CreateWidget<URuinmatesTerminalScreen>(Player, URuinmatesTerminalScreen::StaticClass());
		if (!Screen) return;
		Screen->AddToViewport(40);
		Screen->ShowVisualPreview(Mode, PreviewCapacity);
		FInputModeUIOnly ModeUI;
		ModeUI.SetWidgetToFocus(Screen->TakeWidget());
		Player->SetInputMode(ModeUI);
		Player->SetShowMouseCursor(true);
		return;
	}
}

static FAutoConsoleCommand GPreviewStorage(TEXT("Ruinmates.PreviewStorage"), TEXT("Isolated storage visual preview."),
	FConsoleCommandDelegate::CreateStatic([]() { OpenVisualTerminal(2); }));
static FAutoConsoleCommand GPreviewThirty(TEXT("Ruinmates.PreviewInventory30"), TEXT("Thirty-slot isolated backpack for scroll verification."),
	FConsoleCommandDelegate::CreateStatic([]() { OpenVisualTerminal(0,30); }));
static FAutoConsoleCommand GPreviewMarket(TEXT("Ruinmates.PreviewMarket"), TEXT("Isolated market visual preview; no transactions."),
	FConsoleCommandDelegate::CreateStatic([]() { OpenVisualTerminal(3); }));
static FAutoConsoleCommand GPreviewComponents(TEXT("Ruinmates.UIShowcase"), TEXT("V4 native component and state showcase."),
	FConsoleCommandDelegate::CreateStatic([]() { OpenVisualTerminal(4); }));

static FAutoConsoleCommand GPreviewInventory(
	TEXT("Ruinmates.PreviewInventory"),
	TEXT("Open the isolated terminal backpack preview. Does not use live inventory."),
	FConsoleCommandDelegate::CreateStatic([]() { OpenTerminalPreview(false); }));

static FAutoConsoleCommand GPreviewSearch(
	TEXT("Ruinmates.PreviewSearch"),
	TEXT("Open the isolated terminal search preview. Does not use a live container."),
	FConsoleCommandDelegate::CreateStatic([]() { OpenTerminalPreview(true); }));

#if !UE_BUILD_SHIPPING
// ---- Dev-only UI test hooks -------------------------------------------------------------------
// Drive the live terminal from the console (used by Scripts/UI/capture_terminal_ui.py). They call
// the same public character entry points as the input bindings; nothing here bypasses the server.
namespace RuinmatesUITest
{
	ARuinmatesCharacter* LocalCharacter()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || (Context.WorldType != EWorldType::PIE && Context.WorldType != EWorldType::Game))
			{
				continue;
			}
			if (APlayerController* Player = World->GetFirstPlayerController())
			{
				return Cast<ARuinmatesCharacter>(Player->GetPawn());
			}
		}
		return nullptr;
	}

	int32 FirstArg(const TArray<FString>& Args)
	{
		return Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
	}
}

static FAutoConsoleCommand GUITestBackpack(
	TEXT("Ruinmates.UITest.ToggleBackpack"),
	TEXT("Dev: same as pressing TAB on the local character."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		if (ARuinmatesCharacter* Character = RuinmatesUITest::LocalCharacter())
		{
			Character->ToggleBackpack();
		}
	}));

static FAutoConsoleCommand GUITestSearch(
	TEXT("Ruinmates.UITest.SearchNearest"),
	TEXT("Dev (host only): moves the local pawn next to the nearest loot container and interacts with it."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		ARuinmatesCharacter* Character = RuinmatesUITest::LocalCharacter();
		if (!Character || !Character->HasAuthority())
		{
			UE_LOG(LogTemp, Warning, TEXT("Ruinmates.UITest.SearchNearest needs a local host character."));
			return;
		}
		ARuinmatesLootContainer* Nearest = nullptr;
		float Best = TNumericLimits<float>::Max();
		for (TActorIterator<ARuinmatesLootContainer> It(Character->GetWorld()); It; ++It)
		{
			const float Distance = FVector::DistSquared(It->GetActorLocation(), Character->GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Nearest = *It;
			}
		}
		if (!Nearest)
		{
			UE_LOG(LogTemp, Warning, TEXT("Ruinmates.UITest.SearchNearest: no loot container in the level."));
			return;
		}
		if (!IInteractable::Execute_CanInteract(Nearest, Character))
		{
			const FVector Offset = (Character->GetActorLocation() - Nearest->GetActorLocation()).GetSafeNormal2D() * 150.f;
			Character->TeleportTo(Nearest->GetActorLocation() + Offset + FVector(0.f, 0.f, 100.f), Character->GetActorRotation());
		}
		IInteractable::Execute_Interact(Nearest, Character);
	}));

static FAutoConsoleCommand GUITestPickup(
	TEXT("Ruinmates.UITest.Pickup"),
	TEXT("Dev: same as clicking container card N."),
	FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
	{
		if (ARuinmatesCharacter* Character = RuinmatesUITest::LocalCharacter())
		{
			Character->RequestPickup(RuinmatesUITest::FirstArg(Args));
		}
	}));

static FAutoConsoleCommand GUITestTakeAll(
	TEXT("Ruinmates.UITest.TakeAll"),
	TEXT("Dev: same as the 'take revealed' button."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		if (ARuinmatesCharacter* Character = RuinmatesUITest::LocalCharacter())
		{
			Character->RequestTakeAllLoot();
		}
	}));

static FAutoConsoleCommand GUITestDiscard(
	TEXT("Ruinmates.UITest.Discard"),
	TEXT("Dev: same as right click > discard on backpack slot N."),
	FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
	{
		if (ARuinmatesCharacter* Character = RuinmatesUITest::LocalCharacter())
		{
			Character->RequestDiscard(RuinmatesUITest::FirstArg(Args));
		}
	}));

static FAutoConsoleCommand GUITestClose(
	TEXT("Ruinmates.UITest.Close"),
	TEXT("Dev: same as ESC on the terminal."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		if (ARuinmatesCharacter* Character = RuinmatesUITest::LocalCharacter())
		{
			Character->CloseTerminal();
		}
	}));
#endif
