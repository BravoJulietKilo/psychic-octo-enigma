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
                "CoreUObject",
                "Engine"
            }
        );
        
        PublicIncludePathModuleNames.AddRange(
            new string[]
            {
                
            }
        );
    }
}
