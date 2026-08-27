#include "Enemy/EnemyProjectile.h"
#include "Tower/CentralTower.h"
#include "Defender/DefenderUnit.h"
#include "Enemy/EnemyUnit.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEnemyProjectile::AEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(18.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	// Ignore terrain/pawns so sweeps don't stick on ground or sibling slimes; overlap dynamics for cannons/tower.
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetCanEverAffectNavigation(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		Mesh->SetStaticMesh(SphereAsset.Object);
		Mesh->SetRelativeScale3D(FVector(0.28f));
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMat.Succeeded())
	{
		Mesh->SetMaterial(0, BasicMat.Object);
	}
}

void AEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	Collision->OnComponentBeginOverlap.AddDynamic(this, &AEnemyProjectile::OnOverlap);

	// Slime-green tint so shots read as enemy fire.
	if (UMaterialInstanceDynamic* Mid = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 0.85f, 0.35f));
	}
}

void AEnemyProjectile::InitProjectile(AActor* InTarget, float InDamage, float InSpeed, AActor* InInstigatorActor)
{
	TargetActor = InTarget;
	Damage = InDamage;
	Speed = FMath::Max(InSpeed, 50.f);
	InstigatorActor = InInstigatorActor;

	FVector Dir = FVector::ForwardVector;
	if (InTarget)
	{
		Dir = (InTarget->GetActorLocation() + FVector(0.f, 0.f, 40.f)) - GetActorLocation();
	}
	Dir.Z = FMath::Clamp(Dir.Z, -200.f, 200.f);
	if (Dir.SizeSquared() < 1.f)
	{
		Dir = GetActorForwardVector();
	}
	Velocity = Dir.GetSafeNormal() * Speed;
	SetActorRotation(Velocity.Rotation());
}

void AEnemyProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bConsumed)
	{
		return;
	}

	Age += DeltaTime;
	if (Age >= Lifetime)
	{
		DestroyProjectile();
		return;
	}

	// Soft homing keeps shots on cannons / tower while enemies are moving.
	const FVector AimOffset(0.f, 0.f, 40.f);
	if (AActor* Target = TargetActor.Get())
	{
		const FVector AimPoint = Target->GetActorLocation() + AimOffset;
		const FVector Desired = (AimPoint - GetActorLocation()).GetSafeNormal() * Speed;
		Velocity = FMath::VInterpTo(Velocity, Desired, DeltaTime, HomingStrength);
		if (Velocity.SizeSquared() > 1.f)
		{
			SetActorRotation(Velocity.Rotation());
		}

		// Reliable TD hit: large radius covers cannon footprint + height delta vs path.
		const float HitR = HitProximityRadius;
		if (FVector::DistSquared(GetActorLocation(), AimPoint) <= HitR * HitR
			|| FVector::DistSquared2D(GetActorLocation(), Target->GetActorLocation()) <= HitR * HitR)
		{
			ApplyHitTo(Target);
			return;
		}
	}

	// No sweep — WorldStatic Block would pin shots into hills before they reach elevated pads.
	SetActorLocation(GetActorLocation() + Velocity * DeltaTime, false);
}

void AEnemyProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ApplyHitTo(OtherActor);
}

void AEnemyProjectile::ApplyHitTo(AActor* Other)
{
	if (bConsumed || !Other || Other == this)
	{
		return;
	}
	if (InstigatorActor.IsValid() && Other == InstigatorActor.Get())
	{
		return;
	}
	// Don't clip other slimes / sibling shots.
	if (Cast<AEnemyUnit>(Other) || Cast<AEnemyProjectile>(Other))
	{
		return;
	}

	if (ADefenderUnit* Defender = Cast<ADefenderUnit>(Other))
	{
		if (Defender->IsAlive())
		{
			Defender->ApplyDamage(Damage);
		}
		DestroyProjectile();
		return;
	}

	if (ACentralTower* Tower = Cast<ACentralTower>(Other))
	{
		if (Tower->IsAlive())
		{
			Tower->ApplyDamage(Damage);
		}
		DestroyProjectile();
		return;
	}

	// Ignore terrain / props — lifetime + target proximity handle cleanup (TD shots fly above ground).
}

void AEnemyProjectile::DestroyProjectile()
{
	if (bConsumed)
	{
		return;
	}
	bConsumed = true;
	Destroy();
}
