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
                "GameplayTags",
                "MassEntity",
                "MassCommon"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine"
            }
        );

        // Editor-only dependencies
        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "StateTreeEditorModule"  // Needed for tasks to appear in editor
                }
            );
        }
    }
}
