using UnrealBuildTool;

public class PraxisCore : ModuleRules
{
    public PraxisCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "CoreUObject",
                "Engine"
            }
        );
        
        PublicIncludePathModuleNames.AddRange(
            new string[]
            {
                // ➡️ THIS IS THE KEY FIX: Tells UBT to find headers, but not link
                "PraxisSimulationKernel" 
            }
        );
    }
}