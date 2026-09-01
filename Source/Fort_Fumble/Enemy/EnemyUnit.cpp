// slime pathing, combat, idle/walk anims

#include "Enemy/EnemyUnit.h"
#include "Enemy/EnemyProjectile.h"
#include "Tower/CentralTower.h"
#include "Defender/DefenderUnit.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AEnemyUnit::AEnemyUnit()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(ECC_Pawn);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetSphereRadius(40.f);
	Collision->SetCanEverAffectNavigation(false);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// slime mesh facing - rel yaw +90 then MeshYawOffset 180 so they don't walk backward
	Mesh->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));

	// main enemy visual - slime from monster pack, single-node idle/walk (no anim BP)
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MonsterAsset(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Slime_SK.Slime_SK"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAsset(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Slime/Slime_IdleNormal_ANIM.Slime_IdleNormal_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAsset(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Slime/Slime_Walk_ANIM.Slime_Walk_ANIM"));

	if (MonsterAsset.Succeeded())
	{
		Mesh->SetSkeletalMesh(MonsterAsset.Object);
		const FBoxSphereBounds Bounds = MonsterAsset.Object->GetBounds();
		const float MeshHeight = FMath::Max(Bounds.BoxExtent.Z * 2.f, 1.f);
		BaseMeshScale = FMath::Clamp(TargetHeight / MeshHeight, 0.15f, 2.5f);
		MeshScale = BaseMeshScale;
		Mesh->SetRelativeScale3D(FVector(BaseMeshScale));

		// lift mesh so feet sit on path surface
		const float BottomZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, -BottomZ * BaseMeshScale));

		const float Radius = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * BaseMeshScale * 0.85f;
		Collision->SetSphereRadius(FMath::Clamp(Radius, 28.f, 55.f));
		bUsingMonsterMesh = true;
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("[PortalProtect] Failed to load Slime_SK — enemy will have no skeletal mesh until the MonsterForSurvivalGame pack is present."));
		Collision->SetSphereRadius(35.f);
		bUsingMonsterMesh = false;
	}

	if (IdleAsset.Succeeded())
	{
		IdleAnim = IdleAsset.Object;
	}
	if (WalkAsset.Succeeded())
	{
		WalkAnim = WalkAsset.Object;
	}

	Health = MaxHealth;
}

void AEnemyUnit::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	RefreshDamageVisual();
	UpdateLocomotionAnim(false);
	UE_LOG(LogTemp, Log,
		TEXT("[PortalProtect] Enemy slime facing: Mesh RelYaw=+90, MeshYawOffset=180 (actor faces move/aim dir)."));
}

// spawner passes waypoint list, snap to first path cell
void AEnemyUnit::InitializeOnPath(const TArray<FVector>& InWaypoints)
{
	Waypoints = InWaypoints;
	WaypointIndex = 0;
	bInitialized = Waypoints.Num() > 0;
	if (bInitialized)
	{
		// keep collision center on path surface
		const float GroundZ = Waypoints[0].Z;
		const float Radius = Collision->GetScaledSphereRadius();
		SetActorLocation(FVector(Waypoints[0].X, Waypoints[0].Y, GroundZ + Radius));
	}
}

void AEnemyUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsAlive() || !bInitialized)
	{
		return;
	}

	TryAttackEnemyTargets(DeltaTime);
	MoveAlongPath(DeltaTime);
}

void AEnemyUnit::ApplyDamage(float Amount)
{
	if (!IsAlive())
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Amount);
	RefreshDamageVisual();
	if (Health <= 0.f)
	{
		Destroy();
	}
}

// step toward current waypoint, pause when bCombatEngaged
void AEnemyUnit::MoveAlongPath(float DeltaTime)
{
	// only stop when inside engage range, not just aggro range
	if (bCombatEngaged)
	{
		return;
	}

	if (WaypointIndex >= Waypoints.Num())
	{
		// reached end of path - face tower and shoot from TryAttackEnemyTargets
		if (ACentralTower* Tower = FindTower())
		{
			UpdateFacing(Tower->GetActorLocation() - GetActorLocation(), DeltaTime);
		}
		UpdateLocomotionAnim(false);
		return;
	}

	const FVector Target = Waypoints[WaypointIndex];
	const FVector Current = GetActorLocation();
	const float Radius = Collision->GetScaledSphereRadius();
	const FVector TargetCenter(Target.X, Target.Y, Target.Z + Radius);
	const FVector Delta = TargetCenter - Current;
	const float Dist = Delta.Size();
	const float Step = MoveSpeed * DeltaTime;

	if (Dist > 5.f)
	{
		UpdateFacing(Delta, DeltaTime);
	}

	if (Dist <= Step || Dist < 5.f)
	{
		SetActorLocation(TargetCenter);
		++WaypointIndex;
		UpdateLocomotionAnim(WaypointIndex < Waypoints.Num());
	}
	else
	{
		SetActorLocation(Current + Delta.GetSafeNormal() * Step);
		UpdateLocomotionAnim(true);
	}
}

// pick defender or tower in range, spawn projectile on cooldown
bool AEnemyUnit::TryAttackEnemyTargets(float DeltaTime)
{
	bCombatEngaged = false;
	AttackTimer -= DeltaTime;

	const bool bAtPathEnd = WaypointIndex >= Waypoints.Num();
	const FVector Origin = GetActorLocation();

	// defenders first if in fire range, else tower (wider range at path end)
	const float SelectRange = FMath::Max(AttackRange, DefenderAggroRange);
	ADefenderUnit* Defender = FindNearbyDefender(SelectRange);
	ACentralTower* Tower = FindTower();
	const float EffectiveTowerRange = bAtPathEnd ? FMath::Max(AttackRange, TowerAttackRange) : AttackRange;

	float DefenderDistSq = TNumericLimits<float>::Max();
	if (Defender)
	{
		DefenderDistSq = FVector::DistSquared2D(Origin, Defender->GetActorLocation());
	}
	const bool bDefenderInFireRange = Defender && DefenderDistSq <= AttackRange * AttackRange;

	float TowerDistSq = TNumericLimits<float>::Max();
	const bool bTowerAlive = Tower && Tower->IsAlive();
	if (bTowerAlive)
	{
		TowerDistSq = FVector::DistSquared2D(Origin, Tower->GetActorLocation());
	}
	const bool bTowerInFireRange = bTowerAlive && TowerDistSq <= EffectiveTowerRange * EffectiveTowerRange;

	if (!bDefenderInFireRange && !bTowerInFireRange)
	{
		return false;
	}

	const float StopRange = AttackRange * EngageStopFactor;
	const float TowerStopRange = bAtPathEnd
		? FMath::Max(StopRange, TowerAttackRange * EngageStopFactor)
		: StopRange;
	const bool bCloseEnoughToStop =
		(bDefenderInFireRange && DefenderDistSq <= StopRange * StopRange)
		|| (bTowerInFireRange && TowerDistSq <= TowerStopRange * TowerStopRange);

	if (bCloseEnoughToStop)
	{
		bCombatEngaged = true;
		UpdateLocomotionAnim(false);
	}

	AActor* TargetActor = nullptr;
	FVector FaceDir = FVector::ZeroVector;
	if (bDefenderInFireRange)
	{
		TargetActor = Defender;
		FaceDir = Defender->GetActorLocation() - Origin;
	}
	else if (bTowerInFireRange)
	{
		TargetActor = Tower;
		FaceDir = Tower->GetActorLocation() - Origin;
	}
	UpdateFacing(FaceDir, DeltaTime);

	if (AttackTimer > 0.f || !TargetActor)
	{
		return true;
	}

	FireProjectileAt(TargetActor);
	AttackTimer = AttackCooldown;
	return true;
}

void AEnemyUnit::FireProjectileAt(AActor* Target)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 35.f) + GetActorForwardVector() * 30.f;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = nullptr;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEnemyProjectile* Shot = GetWorld()->SpawnActor<AEnemyProjectile>(
		AEnemyProjectile::StaticClass(), Muzzle, GetActorRotation(), Params);
	if (Shot)
	{
		Shot->InitProjectile(Target, AttackDamage, ProjectileSpeed, this);
	}

	DrawDebugLine(GetWorld(), Muzzle, Target->GetActorLocation() + FVector(0.f, 0.f, 40.f),
		FColor::Orange, false, 0.1f, 0, 2.f);
}

// yaw toward move/combat dir, MeshYawOffset fixes import facing
void AEnemyUnit::UpdateFacing(const FVector& WorldDirection, float DeltaTime)
{
	FVector Flat = WorldDirection;
	Flat.Z = 0.f;
	if (Flat.SizeSquared() < 1.f)
	{
		return;
	}

	FRotator Desired = Flat.Rotation();
	Desired.Yaw += MeshYawOffset;
	const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), Desired, DeltaTime, TurnInterpSpeed);
	SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));
}

void AEnemyUnit::UpdateLocomotionAnim(bool bMoving)
{
	UAnimSequence* Desired = bMoving ? WalkAnim.Get() : IdleAnim.Get();
	if (!Desired || Desired == CurrentAnim)
	{
		return;
	}

	Mesh->PlayAnimation(Desired, true);
	CurrentAnim = Desired;
}

ADefenderUnit* AEnemyUnit::FindNearbyDefender(float Range) const
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADefenderUnit::StaticClass(), Found);

	ADefenderUnit* Best = nullptr;
	float BestDistSq = Range * Range;
	const FVector Origin = GetActorLocation();

	for (AActor* Actor : Found)
	{
		ADefenderUnit* Defender = Cast<ADefenderUnit>(Actor);
		if (!Defender || !Defender->IsAlive())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Origin, Defender->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Defender;
		}
	}
	return Best;
}

ACentralTower* AEnemyUnit::FindTower() const
{
	return Cast<ACentralTower>(UGameplayStatics::GetActorOfClass(GetWorld(), ACentralTower::StaticClass()));
}

void AEnemyUnit::RefreshDamageVisual()
{
	// damage feedback = slight squash, keep pack materials
	const float Ratio = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	const float ScaleMul = FMath::Lerp(0.88f, 1.f, Ratio);
	if (bUsingMonsterMesh)
	{
		Mesh->SetRelativeScale3D(FVector(BaseMeshScale * ScaleMul));
	}
}
