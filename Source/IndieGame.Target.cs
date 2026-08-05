using UnrealBuildTool;
using System.Collections.Generic;

public class IndieGameTarget : TargetRules
{
	public IndieGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("IndieGame");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Keep the Windows executable metadata on the public game version.
			BuildVersion = "1.0.0";
			WindowsPlatform.bSetResourceVersions = true;
		}
	}
}
