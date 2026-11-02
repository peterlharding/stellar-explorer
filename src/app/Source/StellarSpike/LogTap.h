#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"

/**
 * Every log line, from any thread, numbered, the last Backlog kept for the game thread to
 * read: each reader (a console, the panel) takes the lines numbered after the last it took,
 * so a line logged as a reader starts reaches it once, not in the backlog and again as new.
 * Warnings and errors say so after their category ("LogX: Warning: ...").
 */
class FLogTap : public FOutputDevice
{
public:
	/** Lines kept for a reader that comes later. */
	static constexpr int32 Backlog = 500;

	virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
		const TCHAR* Prefix = Level == ELogVerbosity::Error || Level == ELogVerbosity::Fatal ? TEXT("Error: ") : Level == ELogVerbosity::Warning ? TEXT("Warning: ") : TEXT("");
		FScopeLock Guard(&Lock);
		Recent.Add({++Last, FString::Printf(TEXT("%s: %s%s"), *Category.ToString(), Prefix, Text)});
		if (Recent.Num() > Backlog)
		{
			Recent.RemoveAt(0, Recent.Num() - Backlog, EAllowShrinking::No);
		}
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

	/** The lines numbered after After, oldest first; After becomes the last of them. */
	TArray<FString> Since(uint64& After)
	{
		FScopeLock Guard(&Lock);
		TArray<FString> Lines;
		for (const TPair<uint64, FString>& Line : Recent)
		{
			if (Line.Key > After)
			{
				Lines.Add(Line.Value);
			}
		}
		After = Last;
		return Lines;
	}

private:
	FCriticalSection Lock;
	TArray<TPair<uint64, FString>> Recent;
	uint64 Last = 0;
};
