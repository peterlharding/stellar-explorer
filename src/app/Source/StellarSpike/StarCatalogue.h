#pragma once

#include "CoreMinimal.h"

#include "stellar_core.h"

/**
 * A thin RAII wrapper over the core's C catalogue API (ADR 0014), the engine side of
 * the boundary. It opens the catalogue read-only, loads every usable star, and closes
 * the handle on destruction, so the rest of the app works with a TArray, not the C ABI.
 */
class FStarCatalogue
{
public:
	explicit FStarCatalogue(const FString& Path);
	~FStarCatalogue();

	FStarCatalogue(const FStarCatalogue&) = delete;
	FStarCatalogue& operator=(const FStarCatalogue&) = delete;

	bool IsOpen() const { return Handle != nullptr; }
	const TArray<StcStar>& Stars() const { return LoadedStars; }

private:
	StcCatalogue* Handle = nullptr;
	TArray<StcStar> LoadedStars;
};
