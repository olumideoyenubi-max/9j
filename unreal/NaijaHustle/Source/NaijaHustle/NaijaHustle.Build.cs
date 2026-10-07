using UnrealBuildTool;

public class NaijaHustle : ModuleRules
{
	public NaijaHustle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Gameplay is in the NaijaHustleGame plugin; this module is the project shell.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput"
		});
	}
}
