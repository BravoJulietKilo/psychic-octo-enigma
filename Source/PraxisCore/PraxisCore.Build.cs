using UnrealBuildTool;

public class PraxisCore : ModuleRules
{
    public PraxisCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "MassEntity",
                "MassCommon",
                "StructUtils"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "PraxisMass"
            }
        );
        
        PublicIncludePathModuleNames.AddRange(
            new string[]
            {
                
            }
        );
    }
}
