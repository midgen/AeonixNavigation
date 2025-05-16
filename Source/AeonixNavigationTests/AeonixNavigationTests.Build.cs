using UnrealBuildTool;

public class AeonixNavigationTests : ModuleRules
{
    public AeonixNavigationTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bLegacyParentIncludePaths = false;
        CppStandard = CppStandardVersion.Default;
        
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AeonixNavigation" });
    }
}
