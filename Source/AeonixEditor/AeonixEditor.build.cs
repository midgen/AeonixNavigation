using UnrealBuildTool;

public class AeonixEditor : ModuleRules
{
    public AeonixEditor(ReadOnlyTargetRules Target) : base(Target)
    {

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AeonixNavigation", "InputCore" });

        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "PropertyEditor", "EditorStyle", "UnrealEd", "GraphEditor", "BlueprintGraph", "EditorSubsystem" });

        PrivateIncludePaths.AddRange(new string[] { "AeonixEditor/Private" });

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bLegacyParentIncludePaths = false;
        CppStandard = CppStandardVersion.Default;

    }
};