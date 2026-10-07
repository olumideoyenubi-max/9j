using System.IO;
using UnrealBuildTool;

public class NaijaHustleGame : ModuleRules
{
	public NaijaHustleGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput"
		});
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json", "Projects" // reading Data/*.json from the plugin folder
		});

		// the city and rules ship as loose files next to the plugin, in the editor and in packaged games
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Data", "*.json"), StagedFileType.NonUFS);
	}
}
