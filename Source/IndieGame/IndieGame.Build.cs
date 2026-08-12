using UnrealBuildTool;
using System.IO;

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
		// Optional microphone mode reduces capture buffers to a local envelope;
		// the platform backend is loaded by the AudioCapture project plugin.
		PrivateDependencyModuleNames.Add("AudioCaptureCore");

		// Bundle UI typefaces with every target. Loading from the project
		// directory keeps glyph metrics identical in Editor and packaged builds.
		string FontsDirectory = Path.Combine(ModuleDirectory, "UI", "Fonts");
		string[] BundledFontFiles =
		{
			"Pretendard-Regular.otf",
			"Pretendard-SemiBold.otf",
			"GowunBatang-Bold.ttf",
			"OFL-Pretendard.txt",
			"OFL-GowunBatang.txt"
		};
		foreach (string FontFile in BundledFontFiles)
		{
			RuntimeDependencies.Add(
				"$(TargetOutputDir)/UI/Fonts/" + FontFile,
				Path.Combine(FontsDirectory, FontFile),
				StagedFileType.NonUFS);
		}
	}
}
