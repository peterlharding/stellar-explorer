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

/**
 * The branch and when it was built, short enough for the headset's panel, which fits about 28
 * characters: "local-space, 5 Nov 12:15". The time is the build machine's; the branch loses its
 * kind ("feature-"), then if need be its end.
 */
inline FString StellarShortBranchAndTime()
{
	constexpr int32 Fits = 28;
	static const TCHAR* const Months[] = {TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"),
		TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")};
	// STELLAR_SPIKE_BUILT is "2026-11-05 12:15 +1100".
	FString Built = STELLAR_SPIKE_BUILT;
	TArray<FString> Parts;
	Built.ParseIntoArrayWS(Parts);
	TArray<FString> Date;
	if (Parts.Num() >= 2 && Parts[0].ParseIntoArray(Date, TEXT("-")) == 3 && FCString::Atoi(*Date[1]) >= 1 && FCString::Atoi(*Date[1]) <= 12)
	{
		Built = FString::Printf(TEXT("%d %s %s"), FCString::Atoi(*Date[2]), Months[FCString::Atoi(*Date[1]) - 1], *Parts[1]);
	}
	FString Branch = STELLAR_SPIKE_BRANCH;
	const int32 Room = Fits - Built.Len() - 2;
	for (const TCHAR* Kind : {TEXT("feature-"), TEXT("fix-"), TEXT("docs-")})
	{
		if (Branch.Len() > Room && Branch.StartsWith(Kind))
		{
			Branch.RightChopInline(FCString::Strlen(Kind));
		}
	}
	if (Branch.Len() > Room)
	{
		Branch = Branch.Left(FMath::Max(Room - 3, 1)) + TEXT("...");
	}
	return FString::Printf(TEXT("%s, %s"), *Branch, *Built);
}

/** The version and the build stamp, for the log and the console: "Version 0.1.0, build 2e9cca0a0406 on devel, built ...". */
inline FString StellarBuild()
{
	return FString::Printf(TEXT("Version %s, build %s on %s, built %s"), *StellarVersion(), STELLAR_SPIKE_COMMIT, STELLAR_SPIKE_BRANCH, STELLAR_SPIKE_BUILT);
}
