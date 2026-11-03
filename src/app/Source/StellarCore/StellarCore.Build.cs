using System.IO;
using UnrealBuildTool;

// External module exposing the engine-independent core (ADR 0008) to the app through
// its C ABI (ADR 0014). The core lives in <repo>/src/core and is built separately,
// per platform, into <build>/Release (see src/core/README.md); this points the app at
// its headers and the prebuilt static libraries. Build the core for the target first.
public class StellarCore : ModuleRules
{
	public StellarCore(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string CoreDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "core"));
		PublicSystemIncludePaths.Add(Path.Combine(CoreDir, "include"));

		// One build directory per platform (ADR 0013), each with a Release/ subdirectory
		// (VS generator on Windows, Xcode generator on the Mac).
		string BuildSub;
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			BuildSub = "build-msvc";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			BuildSub = "build-mac";
		}
		else if (Target.Platform == UnrealTargetPlatform.VisionOS)
		{
			BuildSub = "build-visionos";
		}
		else
		{
			throw new BuildException("StellarCore: unsupported platform " + Target.Platform + " (ADR 0013).");
		}

		string LibDir = Path.Combine(CoreDir, BuildSub, "Release");
		bool Msvc = Target.Platform == UnrealTargetPlatform.Win64;
		string Prefix = Msvc ? "" : "lib";
		string Ext = Msvc ? ".lib" : ".a";
		foreach (string Name in new[] { "stellar_core", "sqlite3" })
		{
			string LibPath = Path.Combine(LibDir, Prefix + Name + Ext);
			if (!File.Exists(LibPath))
			{
				throw new BuildException(
					"StellarCore: missing " + LibPath + ". Build the core for " + Target.Platform +
					" first (see src/core/README.md).");
			}
			PublicAdditionalLibraries.Add(LibPath);
		}
	}
}
