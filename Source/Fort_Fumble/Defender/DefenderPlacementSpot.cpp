// pad mesh, highlight colors, surface height for spawning cannons

#include "Defender/DefenderPlacementSpot.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ADefenderPlacementSpot::ADefenderPlacementSpot()
{
	PrimaryActorTick.bCanEverTick = false;

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	SetRootComponent(MarkerMesh);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MarkerMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	MarkerMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	MarkerMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		MarkerMesh->SetStaticMesh(CylinderAsset.Object);
		// thin disc ~120uu wide, 15uu tall, origin at center
		MarkerMesh->SetRelativeScale3D(FVector(PadXYScale, PadXYScale, PadZScale));
	}
}

// top of the yellow disc - game mode adds cannon pivot offset after this
FVector ADefenderPlacementSpot::GetPadSurfaceLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, GetPadHalfHeight());
}

void ADefenderPlacementSpot::BeginPlay()
{
	Super::BeginPlay();
	ApplyColor(FLinearColor(0.95f, 0.85f, 0.2f, 1.f));
}

void ADefenderPlacementSpot::SetOccupied(bool bInOccupied)
{
	bOccupied = bInOccupied;
	MarkerMesh->SetVisibility(!bOccupied);
}

void ADefenderPlacementSpot::SetHighlighted(bool bHighlight)
{
	if (bOccupied)
	{
		return;
	}
	ApplyColor(bHighlight ? FLinearColor(0.2f, 1.f, 0.4f) : FLinearColor(0.95f, 0.85f, 0.2f));
}

void ADefenderPlacementSpot::ApplyColor(const FLinearColor& Color)
{
	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MarkerMesh->SetMaterial(0, MID);
	}
}
