#include "UI/RuinmatesFieldHUD.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Character/RuinmatesCharacter.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Stats/StatsComponent.h"
#include "TimerManager.h"
#include "UI/RuinmatesSquadRadio.h"
#include "UI/RuinmatesUIStyle.h"
#include "Weapon/BaseWeapon.h"
#include "Weapon/WeaponInventoryComponent.h"
#include "Weapon/WeaponData.h"

void URuinmatesFieldHUD::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	HUDDesign = WidgetTree->ConstructWidget<UCanvasPanel>();
	if (UCanvasPanelSlot* DesignSlot = Root->AddChildToCanvas(HUDDesign))
	{
		DesignSlot->SetAnchors(FAnchors(0.5f));
		DesignSlot->SetAlignment(FVector2D(0.5f));
		DesignSlot->SetSize(FVector2D(1920,1080));
	}
	Radio = CreateWidget<URuinmatesSquadRadio>(this);
	if (Radio)
	{
		UCanvasPanelSlot* RadioSlot = HUDDesign->AddChildToCanvas(Radio);
		RadioSlot->SetPosition(FVector2D(24,190));
		RadioSlot->SetSize(FVector2D(208,312));
	}
	const TCHAR* Paths[] = {TEXT("Art/HUD/T_UI_Health_Track"), TEXT("Art/HUD/T_UI_Health_Fill"),
		TEXT("Art/HUD/T_UI_Health_Critical"), TEXT("Art/HUD/T_UI_StaminaRing_Track"),
		TEXT("Art/HUD/T_UI_CompassTicks"), TEXT("Art/HUD/T_UI_CompassPointer"),
		TEXT("Art/HUD/T_UI_WeaponSlot_Normal"), TEXT("Art/HUD/T_UI_WeaponSlot_Glow"),
		TEXT("Art/HUD/T_UI_WeaponSlot_Disabled"), TEXT("Icons/System/T_Icon_SYS_Health"),
		TEXT("Icons/System/T_Icon_SYS_Stamina")};
	for (const TCHAR* Path : Paths) Brushes.Add(Path, RuinmatesUI::V4(Path));
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RUINMATES/UI/AssetPackV4/Materials/M_UI_StaminaRing.M_UI_StaminaRing"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		RingMaterial = UMaterialInstanceDynamic::Create(Base, this);
		RingBrush.SetResourceObject(RingMaterial);
		RingBrush.SetImageSize(FVector2D(72,72));
		RingBrush.DrawAs = ESlateBrushDrawType::Image;
	}
}

void URuinmatesFieldHUD::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(GetOwningPlayerPawn()))
	{
		if (UStatsComponent* Stats = Character->GetStats())
		{
			BoundStats = Stats;
			Stats->OnHealthChanged.AddUniqueDynamic(this, &URuinmatesFieldHUD::HealthChanged);
			Stats->OnMaxHealthChanged.AddUniqueDynamic(this, &URuinmatesFieldHUD::HealthChanged);
			Stats->OnStaminaChanged.AddUniqueDynamic(this, &URuinmatesFieldHUD::StaminaChanged);
			Stats->OnMaxStaminaChanged.AddUniqueDynamic(this, &URuinmatesFieldHUD::StaminaChanged);
			HealthChanged(Stats,0,0,nullptr);
			StaminaChanged(Stats,0,0);
		}
	}
	RefreshWeapons();
	// Existing weapons expose no ammo-change delegate. A small snapshot timer updates presentation
	// without rebuilding widgets or polling the inventory on every frame.
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(WeaponRefreshTimer, this, &URuinmatesFieldHUD::RefreshWeapons, 0.1f, true);
}

void URuinmatesFieldHUD::HealthChanged(UStatsComponent* Stats, float, float, AActor*)
{
	if (Stats) Health = FMath::Clamp(Stats->GetHealthNormalized(),0.f,1.f);
}

void URuinmatesFieldHUD::StaminaChanged(UStatsComponent* Stats, float, float)
{
	if (Stats) Stamina = Stats->MaxStamina > 0 ? FMath::Clamp(Stats->Stamina / Stats->MaxStamina,0.f,1.f) : 0.f;
	if (RingMaterial) RingMaterial->SetScalarParameterValue(TEXT("Percent"), Stamina);
}

void URuinmatesFieldHUD::SetPreview(bool bInPreview)
{
	bPreview = bInPreview;
	if (Radio) Radio->SetPreview(bPreview);
	if (RingMaterial) RingMaterial->SetScalarParameterValue(TEXT("Percent"), bPreview ? 0.62f : Stamina);
	RefreshWeapons();
}

void URuinmatesFieldHUD::RefreshWeapons()
{
	WeaponIcons.SetNum(4);
	AmmoLabels.SetNum(4);
	WeaponPresent.Init(false,4);
	EquippedIndex = INDEX_NONE;
	const ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(GetOwningPlayerPawn());
	const UWeaponInventoryComponent* Inventory = Character ? Character->GetWeaponInventory() : nullptr;
	const TCHAR* DemoIcons[] = {TEXT("Derived/Icons/T_Icon_M4A1"),TEXT("Derived/Icons/T_Icon_P226"),TEXT("Derived/Icons/T_Icon_TacticalKnife")};
	for (int32 I = 0; I < 4; ++I)
	{
		ABaseWeapon* Weapon = Inventory ? Inventory->GetInventoryWeapon(I) : nullptr;
		WeaponPresent[I] = bPreview ? I < 3 : IsValid(Weapon);
		AmmoLabels[I].Reset();
		WeaponIcons[I] = FSlateBrush();
		if (bPreview && I < 3)
		{
			WeaponIcons[I] = RuinmatesUI::V4(DemoIcons[I]);
			AmmoLabels[I] = I == 0 ? TEXT("28 / 120") : I == 1 ? TEXT("15 / 45") : TEXT("");
		}
		else if (IsValid(Weapon))
		{
			// Prefer the weapon data's optional HUD portrait; an unset icon keeps the neutral glyph.
			const UWeaponData* Data = Weapon->GetWeaponData();
			UTexture2D* IconTexture = Data ? Data->HUDIcon.LoadSynchronous() : nullptr;
			WeaponIcons[I] = IconTexture
				? RuinmatesUI::ImageFor(IconTexture)
				: RuinmatesUI::V4(TEXT("Icons/System/T_Icon_SYS_Ammo"));
			if (Weapon->GetWeaponKind() != ERuinmatesWeaponKind::Melee)
				AmmoLabels[I] = FString::Printf(TEXT("%d / %d"), Weapon->GetAmmoInMag(), Weapon->GetCarriedAmmo());
			if (Character->GetEquippedWeapon() == Weapon) EquippedIndex = I;
		}
	}
	if (bPreview) EquippedIndex = 0;
}

void URuinmatesFieldHUD::NativeTick(const FGeometry& Geometry, float Delta)
{
	Super::NativeTick(Geometry, Delta);
	const FVector2D Size = Geometry.GetLocalSize();
	if (HUDDesign)
	{
		const float Scale = FMath::Min(Size.X/1920.f,Size.Y/1080.f);
		HUDDesign->SetRenderScale(FVector2D(Scale));
	}
}

int32 URuinmatesFieldHUD::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	const FVector2D Size = G.GetLocalSize();
	const float Scale = FMath::Min(Size.X/1920.f,Size.Y/1080.f);
	const FVector2D Offset = (Size-FVector2D(1920,1080)*Scale)*0.5f;
	auto Geo = [&](float X,float Y,float W,float H) { return G.ToPaintGeometry(FVector2D(W,H)*Scale, FSlateLayoutTransform(Offset+FVector2D(X,Y)*Scale)); };
	auto Box = [&](const TCHAR* Path,float X,float Y,float W,float H) {
		if (const FSlateBrush* Brush=Brushes.Find(Path)) FSlateDrawElement::MakeBox(Out,Layer,Geo(X,Y,W,H),Brush,ESlateDrawEffect::None,FLinearColor::White);
	};
	auto Text = [&](const FString& Value,float X,float Y,int32 FontSize,FLinearColor Color) {
		FSlateFontInfo Font=RuinmatesUI::Font(FontSize);
		Font.Size=FMath::Max(1,FMath::RoundToInt(FontSize*Scale));
		FSlateDrawElement::MakeText(Out,Layer+1,Geo(X,Y,160,32),Value,Font,ESlateDrawEffect::None,Color);
	};
	Box(TEXT("Icons/System/T_Icon_SYS_Health"),32,1013,24,24);
	Box(TEXT("Art/HUD/T_UI_Health_Track"),64,1011,360,28);
	const float HP = bPreview ? 0.86f : Health;
	if (HP > 0)
	{
		Out.PushClip(FSlateClippingZone(Geo(64,1011,360*HP,28)));
		Box(HP<0.25f ? TEXT("Art/HUD/T_UI_Health_Critical") : TEXT("Art/HUD/T_UI_Health_Fill"),64,1011,360,28);
		Out.PopClip();
	}
	if ((bPreview || Stamina < 0.995f) && RingMaterial)
	{
		Box(TEXT("Art/HUD/T_UI_StaminaRing_Track"),900,690,72,72);
		FSlateDrawElement::MakeBox(Out,Layer+1,Geo(900,690,72,72),&RingBrush,ESlateDrawEffect::None,FLinearColor::White);
		Box(TEXT("Icons/System/T_Icon_SYS_Stamina"),924,714,24,24);
	}
	const float Heading = GetOwningPlayer() ? FMath::Fmod(GetOwningPlayer()->GetControlRotation().Yaw+360.f,360.f) : 0.f;
	const TCHAR* Cardinal[] = {TEXT("N"),TEXT("NE"),TEXT("E"),TEXT("SE"),TEXT("S"),TEXT("SW"),TEXT("W"),TEXT("NW")};
	for (int32 I=-5; I<=5; ++I)
	{
		const float Degrees = FMath::Fmod(FMath::FloorToFloat(Heading/15.f)*15.f+I*15.f+360.f,360.f);
		const float X=960.f + (I*15.f - FMath::Fmod(Heading,15.f))*6.f;
		const int32 D=FMath::RoundToInt(Degrees);
		Text(D%45 == 0 ? FString(Cardinal[(D/45)%8]) : FString::FromInt(D),X-12,28,16,
			D%90 == 0 ? RuinmatesUI::Accent() : RuinmatesUI::TextSecondary());
	}
	Box(TEXT("Art/HUD/T_UI_CompassTicks"),510,54,900,18);
	Box(TEXT("Art/HUD/T_UI_CompassPointer"),948,64,24,18);
	Text(FString::Printf(TEXT("%03d"),FMath::RoundToInt(Heading)%360),940,86,14,RuinmatesUI::Accent());
	for (int32 I=0; I<4; ++I)
	{
		const float Y=450.f+I*98.f;
		const bool Present=WeaponPresent.IsValidIndex(I)&&WeaponPresent[I];
		Box(Present ? TEXT("Art/HUD/T_UI_WeaponSlot_Normal") : TEXT("Art/HUD/T_UI_WeaponSlot_Disabled"),1704,Y,184,86);
		if (I==EquippedIndex) Box(TEXT("Art/HUD/T_UI_WeaponSlot_Glow"),1704,Y,184,86);
		Text(FString::FromInt(I+1),1718,Y+8,14,RuinmatesUI::TextSecondary());
		if (Present && WeaponIcons.IsValidIndex(I))
		{
			const FSlateBrush& Brush=WeaponIcons[I];
			const FVector2D ArtSize=Brush.GetImageSize();
			const float Fit=FMath::Min(130.f/FMath::Max(1.f,ArtSize.X),40.f/FMath::Max(1.f,ArtSize.Y));
			const FVector2D DrawSize=ArtSize*Fit;
			FSlateDrawElement::MakeBox(Out,Layer+1,Geo(1796-DrawSize.X/2,Y+18,DrawSize.X,DrawSize.Y),&Brush,ESlateDrawEffect::None,FLinearColor::White);
			Text(AmmoLabels[I],1750,Y+61,16,RuinmatesUI::TextPrimary());
		}
	}
	if (bPreview) Text(TEXT("HUD / VISUAL PREVIEW"),32,1048,14,RuinmatesUI::Accent());
	return Super::NativePaint(Args,G,Cull,Out,Layer+2,Style,bEnabled);
}

void URuinmatesFieldHUD::NativeDestruct()
{
	if (UStatsComponent* Stats=BoundStats.Get())
	{
		Stats->OnHealthChanged.RemoveAll(this);
		Stats->OnMaxHealthChanged.RemoveAll(this);
		Stats->OnStaminaChanged.RemoveAll(this);
		Stats->OnMaxStaminaChanged.RemoveAll(this);
	}
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(WeaponRefreshTimer);
	Super::NativeDestruct();
}

static FAutoConsoleCommand GHudPreview(TEXT("Ruinmates.PreviewHUD"),TEXT("Toggle labelled HUD art samples; never changes gameplay."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		if (!GEngine) return;
		for (const FWorldContext& Context:GEngine->GetWorldContexts())
		{
			UWorld* World=Context.World();
			if (!World || (Context.WorldType!=EWorldType::PIE && Context.WorldType!=EWorldType::Game)) continue;
			if (APlayerController* PC=World->GetFirstPlayerController())
			{
				TArray<UUserWidget*> Existing;
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Existing, URuinmatesFieldHUD::StaticClass(), true);
				for (UUserWidget* Widget : Existing)
				{
					URuinmatesFieldHUD* HUD = Cast<URuinmatesFieldHUD>(Widget);
					if (HUD && HUD->GetOwningPlayer() == PC)
					{
						HUD->SetPreview(!HUD->IsPreview());
						return;
					}
				}
				URuinmatesFieldHUD* Preview=CreateWidget<URuinmatesFieldHUD>(PC);
				if (Preview) { Preview->AddToViewport(21); Preview->SetPreview(true); }
			}
			return;
		}
	}));
