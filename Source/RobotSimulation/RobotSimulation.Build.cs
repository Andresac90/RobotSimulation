using UnrealBuildTool;

public class RobotSimulation : ModuleRules
{
    public RobotSimulation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
    "Core","CoreUObject","Engine","InputCore",
    "ChaosVehicles","UMG","AIModule","NavigationSystem"
});

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate",
            "SlateCore",
            "ApplicationCore",
            "EngineSettings"
        });
    }
}
