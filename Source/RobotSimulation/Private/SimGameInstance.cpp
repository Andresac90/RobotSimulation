#include "SimGameInstance.h"
#include "SimFirstRunSave.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h" 
#include "Engine/Engine.h"


void USimGameInstance::Init()
{
    Super::Init();
    RunFirstLaunchGraphicsSetupIfNeeded();
}

void USimGameInstance::RunFirstLaunchGraphicsSetupIfNeeded()
{
    const FString Slot = FirstRunSlot;
    USimFirstRunSave* Save = nullptr;

    if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
        Save = Cast<USimFirstRunSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (!Save)
        Save = Cast<USimFirstRunSave>(UGameplayStatics::CreateSaveGameObject(USimFirstRunSave::StaticClass()));

    if (Save && !Save->bGraphicsBenchmarked)
    {
        if (GEngine)
        {
            if (UGameUserSettings* GS = GEngine->GetGameUserSettings())
            {
                GS->RunHardwareBenchmark();          // measure CPU/GPU
                GS->ApplyHardwareBenchmarkResults();  // set scalability
                GS->ConfirmVideoMode();
                GS->ApplySettings(false);
                GS->SaveSettings();
            }
        }
        Save->bGraphicsBenchmarked = true;
        UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
    }
}

