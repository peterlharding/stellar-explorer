#include "StarCatalogue.h"

FStarCatalogue::FStarCatalogue(const FString& Path)
{
	Handle = stc_catalogue_open(TCHAR_TO_UTF8(*Path));
	if (Handle == nullptr)
	{
		return;
	}
	const int64 Count = stc_catalogue_star_count(Handle);
	if (Count > 0)
	{
		LoadedStars.SetNumUninitialized(static_cast<int32>(Count));
		const int32 Written = stc_catalogue_load_stars(Handle, LoadedStars.GetData(), LoadedStars.Num());
		LoadedStars.SetNum(Written, EAllowShrinking::No);
	}
}

FStarCatalogue::~FStarCatalogue()
{
	stc_catalogue_close(Handle);
}
