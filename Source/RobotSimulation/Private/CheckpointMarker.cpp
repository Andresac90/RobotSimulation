// .cpp
#include "CheckpointMarker.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"

ACheckpointMarker::ACheckpointMarker()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Root);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetMobility(EComponentMobility::Movable);

    // default sphere from Engine content
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded()) Mesh->SetStaticMesh(SphereMesh.Object);
    Mesh->SetRelativeScale3D(FVector(0.5f));           // size it up/down as you like
    Mesh->SetRenderCustomDepth(true);                  // nice outline if you enable it in project

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(Root);
    Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    Label->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    Label->SetWorldSize(80.f);
    Label->SetRelativeLocation(FVector(0, 0, 120.f));
}

void ACheckpointMarker::InitMarker(int32 Index)
{
    Label->SetText(FText::AsNumber(Index));
}
