// first-person pawn - just movement and camera
// placement and pause live on the player controller
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PortalProtectPawn.generated.h"

class UCameraComponent;

UCLASS()
class FORT_FUMBLE_API APortalProtectPawn : public ACharacter
{
	GENERATED_BODY()

public:
	APortalProtectPawn();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float LookSensitivity = 1.f;

protected:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void LookYaw(float Value);
	void LookPitch(float Value);
};
