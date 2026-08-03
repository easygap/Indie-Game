using UnrealBuildTool;

public class IndieGame : ModuleRules
{
	public IndieGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		// Keep feature folders directly under the module root while exposing the
		// same stable include paths to every translation unit.
		PublicIncludePaths.Add(ModuleDirectory);
		PrivateIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"AudioExtensions",
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"LevelSequence",
			// Runtime composite font (Korean HUD text) uses SlateCore types.
			"Slate",
			"SlateCore",
			// Photo-prop meshes are resolved by path at runtime.
			"AssetRegistry"
		});

		PrivateDependencyModuleNames.Add("Json");
		PrivateDependencyModuleNames.Add("PhysicsCore");
	}
}
