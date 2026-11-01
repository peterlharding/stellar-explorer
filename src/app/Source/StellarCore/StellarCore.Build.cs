using System.IO;
using UnrealBuildTool;

// External module exposing the engine-independent core (ADR 0008) to the app through
// its C ABI (ADR 0014). The core lives in <repo>/src/core and is built separately,
// per platform (see src/core/README.md); this points the app at its headers and the
// prebuilt static libraries. Build the core first.
public class StellarCore : ModuleRules
{
	public StellarCore(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string CoreDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "core"));
		PublicSystemIncludePaths.Add(Path.Combine(CoreDir, "include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Produced by: cmake -G "Visual Studio 17 2022" -A x64 ... --config Release
			string LibDir = Path.Combine(CoreDir, "build-msvc", "Release");
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "stellar_core.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "sqlite3.lib"));
		}
		else
		{
			// macOS and visionOS libraries are built on the Mac (ADR 0014); add their
			// paths here once they exist.
			throw new BuildException(
				"StellarCore: no prebuilt core library for " + Target.Platform +
				" yet. Build it (see src/core/README.md) and add its path to StellarCore.Build.cs.");
		}
	}
}
