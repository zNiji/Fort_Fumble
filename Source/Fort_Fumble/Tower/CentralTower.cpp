// portal objective - HP and passive shooting at nearby slimes

#include "Tower/CentralTower.h"
#include "Enemy/EnemyUnit.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ACentralTower::ACentralTower()
{
	PrimaryActorTick.bCanEverTick = true;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	SetRootComponent(BaseMesh);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BaseMesh->SetCollisionObjectType(ECC_WorldStatic);
	BaseMesh->SetCollisionResponseToAllChannels(ECR_Block);
	// enemy projectiles are WorldDynamic overlap - need overlap on portal for damage
	BaseMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	BaseMesh->SetGenerateOverlapEvents(true);

	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(BaseMesh);
	TurretMesh->SetVisibility(false);
	TurretMesh->SetHiddenInGame(true);
	TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PortalAsset(
		TEXT("/Game/RPGTinyFantasyForest/Mesh/BuildingUtilityDeco/SM_PortalA.SM_PortalA"));
	if (PortalAsset.Succeeded())
	{
		BaseMesh->SetStaticMesh(PortalAsset.Object);
		BaseVisualScale = 1.35f;
		BaseMesh->SetRelativeScale3D(FVector(BaseVisualScale));
	}
	else
	{
		// fallback cylinder if forest pack portal mesh is missing
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylinderAsset.Succeeded())
		{
			BaseMesh->SetStaticMesh(CylinderAsset.Object);
			BaseVisualScale = 1.6f;
			BaseMesh->SetRelativeScale3D(FVector(BaseVisualScale, BaseVisualScale, 2.2f));
		}
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PortalMat(
		TEXT("/Game/RPGTinyFantasyForest/Material/MI_Portal01.MI_Portal01"));
	if (PortalMat.Succeeded())
	{
		PortalMaterial = PortalMat.Object;
		BaseMesh->SetMaterial(0, PortalMaterial);
	}

	Health = MaxHealth;
}

void ACentralTower::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	ApplyVisualColor();
}

void ACentralTower::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsAlive())
	{
		return;
	}

	// portal doesn't rotate - just shoots on cooldown
	AttackTimer -= DeltaTime;
	if (AttackTimer <= 0.f)
	{
		TryAttack();
		AttackTimer = AttackCooldown;
	}
}

// take damage, shrink visual a bit, broadcast destroy at zero
void ACentralTower::ApplyDamage(float Amount)
{
	if (!IsAlive())
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Amount);
	ApplyVisualColor();

	if (Health <= 0.f)
	{
		OnTowerDestroyed.Broadcast();
	}
}

// instant damage to nearest slime in range (debug line shows the shot)
void ACentralTower::TryAttack()
{
	if (AEnemyUnit* Target = FindNearestEnemy())
	{
		Target->ApplyDamage(AttackDamage);
		DrawDebugLine(GetWorld(), GetActorLocation() + FVector(0, 0, 160), Target->GetActorLocation(),
			FColor::Cyan, false, 0.15f, 0, 4.f);
	}
}

AEnemyUnit* ACentralTower::FindNearestEnemy() const
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
		const float DistSq = FVector::DistSquared2D(Origin, Enemy->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Enemy;
		}
	}
	return Best;
}

void ACentralTower::ApplyVisualColor()
{
	// pack portal mat has no color param - pulse scale instead for damage feedback
	const float Ratio = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	const float Scale = FMath::Lerp(BaseVisualScale * 0.85f, BaseVisualScale, Ratio);
	BaseMesh->SetRelativeScale3D(FVector(Scale));

	if (PortalMaterial)
	{
		BaseMesh->SetMaterial(0, PortalMaterial);
	}
}
