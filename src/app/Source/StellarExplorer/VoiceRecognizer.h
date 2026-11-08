#pragma once

#include "CoreMinimal.h"

/**
 * The platform's speech recogniser behind one interface: Apple's Speech framework, on
 * the device, on the Mac and the Vision Pro; none yet on Windows. It listens to the
 * microphone and reports the text of one utterance at a time, partial results as the
 * words come and a final one when the utterance ends.
 *
 * Called on the game thread. The recogniser works on threads of its own and queues what
 * it hears for Poll.
 */
class FVoiceRecognizer
{
public:
	struct FHeard
	{
		/** Everything heard in this utterance so far. */
		FString Text;
		/** The utterance has ended: by Finish, by a long silence, or by an error (with Error set). */
		bool bFinal = false;
		FString Error;
	};

	/** The recogniser for this platform, or null where there is none. */
	static TUniquePtr<FVoiceRecognizer> Create();

	virtual ~FVoiceRecognizer() = default;

	/**
	 * Ask for the microphone and speech recognition, and start listening. Phrases (star
	 * names, most likely first) bias recognition towards them. Whether it worked comes
	 * later, through Poll and GetStatus: permission is asked for asynchronously.
	 */
	virtual void Start(const TArray<FString>& Phrases) = 0;

	/** Drop the current utterance and start a new one, from silence. */
	virtual void Restart() = 0;

	/** End the current utterance where it is: a final result follows. */
	virtual void Finish() = 0;

	/** The next thing heard, oldest first; false when there is nothing new. */
	virtual bool Poll(FHeard& Out) = 0;

	/** Whether it is listening, or why it is not, for the panel and the log. */
	virtual FString GetStatus() const = 0;
};
