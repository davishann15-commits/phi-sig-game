using UnrealBuildTool;
public class SeniorSendoff : ModuleRules
{
    public SeniorSendoff(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "ProceduralMeshComponent", "MeshDescription", "StaticMeshDescription" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "UMG", "MoviePlayer", "Sockets", "Json", "HairStrandsCore", "ChaosCloth", "ClothingSystemRuntimeCommon", "ClothingSystemRuntimeInterface", "AnimGraphRuntime", "AnimationCore", "LiveLinkAnimationCore", "PhysicsCore", "RenderCore", "RHI" });
        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.AddRange(new[] { "ClothingSystemEditor", "ClothingSystemEditorInterface", "MetaHumanCharacter", "MetaHumanCharacterEditor", "MetaHumanCoreTechLib", "RigLogicModule" });
    }
}
