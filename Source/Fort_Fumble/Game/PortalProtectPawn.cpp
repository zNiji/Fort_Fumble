#include "Game/PortalProtectPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

APortalProtectPawn::APortalProtectPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->JumpZVelocity = 420.f;
	Move->AirControl = 0.25f;
	Move->MaxWalkSpeed = 600.f;
	Move->MinAnalogWalkSpeed = 20.f;
	Move->BrakingDecelerationWalking = 2000.f;
	Move->SetWalkableFloorAngle(50.f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f)); // eye height
	FirstPersonCamera->bUsePawnControlRotation = true;
}

void APortalProtectPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxisKey(EKeys::W, this, &APortalProtectPawn::MoveForward);
	PlayerInputComponent->BindAxisKey(EKeys::S, this, &APortalProtectPawn::MoveBackward);
	PlayerInputComponent->BindAxisKey(EKeys::D, this, &APortalProtectPawn::MoveRight);
	PlayerInputComponent->BindAxisKey(EKeys::A, this, &APortalProtectPawn::MoveLeft);
	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &APortalProtectPawn::LookYaw);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &APortalProtectPawn::LookPitch);

	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &ACharacter::StopJumping);
}

void APortalProtectPawn::MoveForward(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void APortalProtectPawn::MoveBackward(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorForwardVector(), -Value);
	}
}

void APortalProtectPawn::MoveRight(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void APortalProtectPawn::MoveLeft(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorRightVector(), -Value);
	}
}

void APortalProtectPawn::LookYaw(float Value)
{
	AddControllerYawInput(Value * LookSensitivity);
}

void APortalProtectPawn::LookPitch(float Value)
{
	AddControllerPitchInput(-Value * LookSensitivity);
}
