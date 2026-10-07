using UnrealBuildTool;

public class NaijaHustleEditorTarget : TargetRules
{
	public NaijaHustleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NaijaHustle");
	}
}
