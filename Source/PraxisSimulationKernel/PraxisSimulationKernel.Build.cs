using UnrealBuildTool;

public class PraxisSimulationKernel : ModuleRules
{
    public PraxisSimulationKernel(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "AIModule",
                "PraxisCore",
                "StateTreeModule",          // Core StateTree types
                "GameplayStateTreeModule",  // UStateTreeComponent
                "GameplayTags"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine"
            }
        );
    }
}
