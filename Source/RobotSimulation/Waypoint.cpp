#include "Waypoint.h"

AWaypoint::AWaypoint()
{
    PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
    // Editor‐only billboard so you can see the waypoint in the level
    SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>("SpriteComponent");
    RootComponent = SpriteComponent;
#endif
}
