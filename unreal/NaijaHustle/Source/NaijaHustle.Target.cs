using UnrealBuildTool;

public class NaijaHustleTarget : TargetRules
{
	public NaijaHustleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NaijaHustle");
	}
}
