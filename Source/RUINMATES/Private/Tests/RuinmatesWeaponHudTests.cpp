// Optional weapon HUD portrait data. Presentation only; no gameplay or replication involvement.
#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "UI/RuinmatesUIStyle.h"
#include "Weapon/WeaponData.h"

#if WITH_DEV_AUTOMATION_TESTS
// The weapon HUD portrait is optional data; an unset icon keeps the neutral glyph.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesWeaponHudIconTest, "RuinMates.UI.V4.WeaponHudIcon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesWeaponHudIconTest::RunTest(const FString& Parameters)
{
	UWeaponData* Data = NewObject<UWeaponData>();
	if (!TestNotNull(TEXT("Weapon data can be created"), Data))
	{
		return false;
	}

	TestTrue(TEXT("HUD icon is optional and defaults to unset"), Data->HUDIcon.IsNull());
	TestTrue(TEXT("An unset icon produces no brush resource"),
		RuinmatesUI::ImageFor(nullptr).GetResourceObject() == nullptr);

	Data->HUDIcon = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/RUINMATES/UI/AssetPackV4/Derived/Icons/T_Icon_M4A1.T_Icon_M4A1")));
	UTexture2D* Texture = Data->HUDIcon.LoadSynchronous();
	if (TestNotNull(TEXT("A configured weapon icon loads"), Texture))
	{
		TestEqual(TEXT("Weapon icon is a UI texture"),
			static_cast<int32>(Texture->LODGroup), static_cast<int32>(TEXTUREGROUP_UI));
		TestTrue(TEXT("The image brush uses the configured texture"),
			RuinmatesUI::ImageFor(Texture).GetResourceObject() == Texture);
	}
	return true;
}
#endif