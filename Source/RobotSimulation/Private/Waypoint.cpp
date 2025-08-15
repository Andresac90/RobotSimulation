// .cpp
#include "Waypoint.h"
AWaypoint::AWaypoint()
{
    PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITORONLY_DATA
    SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>("SpriteComponent");
    RootComponent = SpriteComponent;
#endif
}
