#pragma once

#include "CoreMinimal.h"
#include "GeneralProjectSettings.h"

// The commit this was built from, its branch and when, written into BuildStamp.inl by
// spike.sh before each build. "+changes" after the commit marks a working tree that
// differed from it.
#if __has_include("BuildStamp.inl")
#include "BuildStamp.inl"
#else
#define STELLAR_SPIKE_COMMIT TEXT("unstamped")
#define STELLAR_SPIKE_BRANCH TEXT("built without spike.sh")
#define STELLAR_SPIKE_BUILT TEXT("at an unknown time")
#endif

/** The release version, from VERSION through ProjectVersion in DefaultGame.ini (scripts/version.py). */
inline FString StellarVersion()
{
	return GetDefault<UGeneralProjectSettings>()->ProjectVersion;
}

/** The commit as git abbreviates it, seven characters, with any "+changes" after: for the headset's panel, where space is short. */
inline FString StellarShortCommit()
{
	const FString Stamp = STELLAR_SPIKE_COMMIT;
	FString Commit;
	FString Changes;
	if (!Stamp.Split(TEXT("+"), &Commit, &Changes))
	{
		return Stamp.Left(7);
	}
	return Commit.Left(7) + TEXT("+") + Changes;
}

/** The version and the build stamp, for the log and the console: "Version 0.1.0, build 2e9cca0a0406 on devel, built ...". */
inline FString StellarBuild()
{
	return FString::Printf(TEXT("Version %s, build %s on %s, built %s"), *StellarVersion(), STELLAR_SPIKE_COMMIT, STELLAR_SPIKE_BRANCH, STELLAR_SPIKE_BUILT);
}
