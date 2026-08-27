#include "Defender/DefenderUnit.h"
#include "Enemy/EnemyUnit.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ADefenderUnit::ADefenderUnit()
{
	PrimaryActorTick.bCanEverTick = true;

	// Scene root so Mesh RelativeRotation survives SetActorRotation in UpdateAim.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	// Enemy projectiles are WorldDynamic + Overlap — Block vs Overlap never generates BeginOverlap.
	Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Mesh->SetGenerateOverlapEvents(true);

	// Cannon.uasset is a MaterialInstanceConstant named "Cannon" — NOT a StaticMesh.
	// The imported FBX StaticMesh lives at CannonSketchfab.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CannonAsset(
		TEXT("/Game/cartoon_cannon_low_poly__extracted/source/CannonSketchfab.CannonSketchfab"));
	if (CannonAsset.Succeeded())
	{
		Mesh->SetStaticMesh(CannonAsset.Object);
		// CannonSketchfab ApproxSize ~101x125x101 (already cm). Old scale 55 filled the map.
		// Fit slightly larger than the yellow pad (~120uu diameter from 1.2× cylinder).
		const FBoxSphereBounds Bounds = CannonAsset.Object->GetBounds();
		const float MeshFootprint = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.f;
		constexpr float TargetFootprint = 140.f;
		BaseMeshScale = FMath::Clamp(TargetFootprint / FMath::Max(MeshFootprint, 1.f), 0.3f, 2.f);
		Mesh->SetRelativeScale3D(FVector(BaseMeshScale));
		// RelYaw −90 was still wrong per playtest. Flip to +90 so barrel/protrusion = actor +X.
		// AimYawOffset stays 0: UpdateAim rotates the actor toward the target; mesh offset holds.
		Mesh->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		PivotToGroundOffset = (Bounds.BoxExtent.Z - Bounds.Origin.Z) * BaseMeshScale;
		bUsingCannonMesh = true;
		UE_LOG(LogTemp, Log,
			TEXT("[PortalProtect] CannonSketchfab facing: Mesh RelYaw=+90 (child of Root), AimYawOffset=0."));
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("[PortalProtect] Failed to load defender cannon StaticMesh at /Game/cartoon_cannon_low_poly__extracted/source/CannonSketchfab.CannonSketchfab — using cone fallback."));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
		if (ConeAsset.Succeeded())
		{
			Mesh->SetStaticMesh(ConeAsset.Object);
			BaseMeshScale = 1.1f;
			Mesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.3f) * BaseMeshScale);
			const FBoxSphereBounds Bounds = ConeAsset.Object->GetBounds();
			PivotToGroundOffset = (Bounds.BoxExtent.Z - Bounds.Origin.Z) * BaseMeshScale * 1.3f;
		}
		bUsingCannonMesh = false;
	}

	Health = MaxHealth;
}

void ADefenderUnit::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	RefreshColor();
}

void ADefenderUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsAlive())
	{
		return;
	}

	// Visual tracking every frame — independent of attack cooldown / damage timing.
	UpdateAim(DeltaTime);

	AttackTimer -= DeltaTime;
	if (AttackTimer <= 0.f)
	{
		TryAttack();
		AttackTimer = AttackCooldown;
	}
}

void ADefenderUnit::ApplyDamage(float Amount)
{
	if (!IsAlive())
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Amount);
	RefreshColor();
	if (Health <= 0.f)
	{
		Destroy();
	}
}

void ADefenderUnit::UpdateAim(float DeltaTime)
{
	AEnemyUnit* Target = FindNearestEnemy();
	if (!Target)
	{
		return; // Hold last facing when idle.
	}

	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	ToTarget.Z += 40.f; // Aim toward torso / center mass, not feet.
	if (ToTarget.SizeSquared() < 1.f)
	{
		return;
	}

	// Actor forward (+X) faces the enemy; Mesh RelYaw keeps the barrel on that axis.
	FRotator Desired = ToTarget.Rotation();
	Desired.Yaw += AimYawOffset;
	Desired.Pitch = FMath::Clamp(Desired.Pitch, -AimMaxPitch, AimMaxPitch);
	Desired.Roll = 0.f;

	const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), Desired, DeltaTime, AimInterpSpeed);
	SetActorRotation(NewRot);
}

void ADefenderUnit::TryAttack()
{
	if (AEnemyUnit* Target = FindNearestEnemy())
	{
		Target->ApplyDamage(AttackDamage);
		DrawDebugLine(GetWorld(), GetActorLocation() + FVector(0, 0, 80), Target->GetActorLocation(),
			FColor::Green, false, 0.12f, 0, 3.f);
	}
}

AEnemyUnit* ADefenderUnit::FindNearestEnemy() const
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyUnit::StaticClass(), Found);

	AEnemyUnit* Best = nullptr;
	float BestDistSq = AttackRange * AttackRange;
	const FVector Origin = GetActorLocation();

	for (AActor* Actor : Found)
	{
		AEnemyUnit* Enemy = Cast<AEnemyUnit>(Actor);
		if (!Enemy || !Enemy->IsAlive())
		{
			continue;
		}
		// 2D range so elevated pads still reach path enemies (matches EnemyUnit).
		const float DistSq = FVector::DistSquared2D(Origin, Enemy->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Enemy;
		}
	}
	return Best;
}

void ADefenderUnit::RefreshColor()
{
	// Keep pack materials intact; show damage via a mild scale dip instead of recoloring.
	const float Ratio = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	const float ScaleMul = FMath::Lerp(0.92f, 1.f, Ratio);
	if (bUsingCannonMesh)
	{
		Mesh->SetRelativeScale3D(FVector(BaseMeshScale * ScaleMul));
	}
	else
	{
		Mesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.3f) * (BaseMeshScale * ScaleMul));
	}
}
