using UnrealBuildTool;
public class SeniorSendoff : ModuleRules
{
    public SeniorSendoff(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Scene generators intentionally use file-local helpers with common names.
        // Compile separately so adding a source file cannot merge those namespaces.
        bUseUnity = false;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "ProceduralMeshComponent", "MeshDescription", "StaticMeshDescription", "AudioMixer", "NetCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "UMG", "MoviePlayer", "Sockets", "Json", "HairStrandsCore", "ChaosCloth", "ClothingSystemRuntimeCommon", "ClothingSystemRuntimeInterface", "AnimGraphRuntime", "AnimationCore", "LiveLinkAnimationCore", "PhysicsCore", "RenderCore", "RHI" });
        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.AddRange(new[] { "ClothingSystemEditor", "ClothingSystemEditorInterface", "MetaHumanCharacter", "MetaHumanCharacterEditor", "MetaHumanCoreTechLib", "RigLogicModule" });
    }
}
