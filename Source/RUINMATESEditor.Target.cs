// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class RUINMATESEditorTarget : TargetRules
{
	public RUINMATESEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;

		ExtraModuleNames.AddRange(new string[] { "RUINMATES", "RUINMATESEditor" });
	}
}
