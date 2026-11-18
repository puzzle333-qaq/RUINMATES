#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponMountComponent.generated.h"

class USkeletalMeshComponent;

// Third-person weapon mount: socket, calibrated offset, and remount idempotency.
// Extracted from the 9aa225f 3P attach path. No first-person mesh and no 46775be remount attempt.
UCLASS(ClassGroup = (RuinMates), meta = (BlueprintSpawnableComponent))
class RUINMATES_API UWeaponMountComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponMountComponent();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Mount")
	void SetPawnMesh(USkeletalMeshComponent* InPawnMesh);

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Mount")
	void SetWeaponMesh(USkeletalMeshComponent* InWeaponMesh);

	UFUNCTION(BlueprintPure, Category = "RuinMates|Mount")
	USkeletalMeshComponent* GetPawnMesh() const { return PawnMesh; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Mount")
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Mount")
	void AttachToOwningPawn();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Mount")
	void AttachToPawnSocket(USkeletalMeshComponent* InPawnMesh, FName SocketName);

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Mount")
	void DetachFromPawn();

	UFUNCTION(BlueprintPure, Category = "RuinMates|Mount")
	static bool IsUsableAttachPoint(const USkeletalMeshComponent* Mesh, FName SocketOrBone);

	// Preferred 3P socket (typically hand_r). When usable, this is the calibrated mount.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Mount")
	FName Mesh3PAttachSocket = FName(TEXT("hand_r"));

	// Complete socket-local mount applied every time the gun is on Mesh3PAttachSocket.
	// Reapplied on re-grip; never accumulated.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Mount")
	FTransform Mesh3PAttachTransform = FTransform::Identity;

	// Fallback requested grip when Mesh3PAttachSocket is missing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Mount")
	FName DefaultGripSocket = FName(TEXT("hand_r"));

protected:
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> PawnMesh;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(Transient)
	FTransform AuthoredWeaponRelative = FTransform::Identity;

	UPROPERTY(Transient)
	bool bCapturedAuthoredRelative = false;

	void CaptureAuthoredMeshRelative();
	void ApplyMeshAttachRelative();
	static FName ResolveAttachPoint(const USkeletalMeshComponent* Mesh, FName Requested);
	static bool IsRightHandGrip(const USkeletalMeshComponent* Mesh, FName SocketName);
};
