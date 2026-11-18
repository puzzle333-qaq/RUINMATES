#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapon/WeaponTypes.h"
#include "WeaponPoseComponent.generated.h"

class USkeletalMeshComponent;

// Third-person left-hand FABRIK target. No first-person pose path and no attachment-slot lookup.
UCLASS(ClassGroup = (RuinMates), meta = (BlueprintSpawnableComponent))
class RUINMATES_API UWeaponPoseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponPoseComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Pose")
	void UpdatePose(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Pose")
	void SetBodyMesh(USkeletalMeshComponent* InBodyMesh);

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Pose")
	void SetWeaponMesh(USkeletalMeshComponent* InWeaponMesh);

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Pose")
	void SetWeaponKind(ERuinmatesWeaponKind InKind) { WeaponKind = InKind; }

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Pose")
	void SetReloading(bool bInReloading) { bIsReloading = bInReloading; }

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Pose")
	void SetPreferredFabrikSocket(FName InSocket) { PreferredFabrikSocket = InSocket; }

	// Resolves an authored left-hand socket on the weapon mesh. Returns false when meshes or sockets are missing.
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Pose")
	bool GetFabrikWorldTransform(FTransform& OutWorld) const;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Pose")
	FTransform GetFabrikSocketTransform() const { return FabrikSocketTransform; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Pose")
	float GetFabrikAlpha() const { return FabrikAlpha; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Pose")
	USkeletalMeshComponent* GetBodyMesh() const { return BodyMesh; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Pose")
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Pose")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Pose")
	ERuinmatesWeaponKind GetWeaponKind() const { return WeaponKind; }


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Pose")
	bool bDisableLeftHandIKDuringReload = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Pose", meta = (ClampMin = "0"))
	float LeftHandIKBlendSpeed = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Pose")
	FName DisableLeftHandIKCurveName = NAME_None;

protected:
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> BodyMesh;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(Transient)
	ERuinmatesWeaponKind WeaponKind = ERuinmatesWeaponKind::Rifle;

	UPROPERTY(Transient)
	bool bIsReloading = false;

	UPROPERTY(EditDefaultsOnly, Category = "RuinMates|Pose")
	FName PreferredFabrikSocket = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Pose")
	FTransform FabrikSocketTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Pose")
	float FabrikAlpha = 0.f;

	bool ComponentHasSocket(const USceneComponent* Component, FName SocketName) const;
	void WriteHandSpace(const FTransform& World, FTransform& Out) const;
};
