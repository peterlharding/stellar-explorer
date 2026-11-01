#pragma once

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
