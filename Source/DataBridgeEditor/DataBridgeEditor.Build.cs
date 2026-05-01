using UnrealBuildTool;

public class DataBridgeEditor : ModuleRules
{
	public DataBridgeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"LevelEditor",
			"Settings",
			"DataBridge",
		});
	}
}
