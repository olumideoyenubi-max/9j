using UnrealBuildTool;

public class NaijaHustle : ModuleRules
{
	public NaijaHustle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Later steps add: ChaosVehicles (step 4), MassEntity/MassAI/ZoneGraph (step 5), UMG/CommonUI (step 6).
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput"
		});
	}
}
