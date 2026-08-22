// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class FrameVolume : ModuleRules
{
	public FrameVolume(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// One runtime module and nothing else.
		//
		// Deliberately NOT here:
		//   UMG      - the product is a camera, and a camera has no widgets. The counters box is drawn on
		//              UCanvas from AHUD so it survives a cooked Shipping build. The demo map's buttons are
		//              UMG assets in Content that call the Blueprint library, exactly as a project would.
		//   Niagara  - nothing here is a particle.
		//   Chaos    - the only physics touched is one optional sphere sweep per tick, which is a plain
		//              world query and needs no physics module of its own.
		//   UnrealEd - everything here ships. There is no editor module, so nothing can go missing between
		//              what a designer frames in the editor and what the packaged game shows.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
		});

		// RenderCore gives us GWhiteTexture, the one-pixel texture the counters box is tiled from.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
		});
	}
}
