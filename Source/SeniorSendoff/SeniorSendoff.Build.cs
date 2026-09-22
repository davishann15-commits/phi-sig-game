using UnrealBuildTool;
public class SeniorSendoff : ModuleRules
{
    public SeniorSendoff(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "UMG", "MoviePlayer", "Sockets", "HairStrandsCore", "ChaosCloth", "ClothingSystemRuntimeCommon", "ClothingSystemRuntimeInterface", "AnimGraphRuntime", "AnimationCore" });
        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.AddRange(new[] { "ClothingSystemEditor", "ClothingSystemEditorInterface", "MeshDescription", "StaticMeshDescription" });
    }
}
