#include "VoiceRecognizer.h"

#if PLATFORM_MAC || PLATFORM_VISIONOS

// Objective-C, compiled as Objective-C++, with manual reference counting like the engine's
// own: ARC would need a precompiled header of its own. The blocks retain the listener, and
// stop releases what holds them, so it lives until the last callback has run.
// The SDK's Carbon headers define an FVector of their own: rename theirs while they are
// read, as the engine's Mac/MacSystemIncludes.h does.
#include "Apple/PreAppleSystemHeaders.h"
#pragma push_macro("FVector")
#define FVector FVectorWorkaround
#import <AVFoundation/AVFoundation.h>
#import <Speech/Speech.h>
#pragma pop_macro("FVector")
#include "Apple/PostAppleSystemHeaders.h"

/**
 * The Speech framework's recogniser on the microphone. The audio engine's tap feeds
 * whichever request is current; restart swaps in a new request, and results from an
 * older one (counted by generation) are dropped. Everything shared is guarded by
 * @synchronized (self), since the tap, the recogniser's callbacks and the game thread
 * all reach it.
 */
@interface StellarExplorerListener : NSObject
@property (atomic, copy) NSString* status;
- (instancetype)initWithPhrases:(NSArray<NSString*>*)phrases;
- (void)start;
- (void)restart;
- (void)finish;
- (void)stop;
/** What was heard since the last call: each is @[text, @(final), error]. */
- (NSArray<NSArray*>*)drain;
@end

@implementation StellarExplorerListener
{
	NSArray<NSString*>* _phrases;
	SFSpeechRecognizer* _recognizer;
	AVAudioEngine* _engine;
	SFSpeechAudioBufferRecognitionRequest* _request;
	SFSpeechRecognitionTask* _task;
	NSMutableArray<NSArray*>* _pending;
	NSInteger _generation;
	BOOL _running;
}

- (instancetype)initWithPhrases:(NSArray<NSString*>*)phrases
{
	if ((self = [super init]))
	{
		_phrases = [phrases copy];
		_pending = [[NSMutableArray alloc] init];
		self.status = @"Asking for the microphone";
	}
	return self;
}

- (void)dealloc
{
	[_phrases release];
	[_recognizer release];
	[_engine release];
	[_request release];
	[_task release];
	[_pending release];
	[_status release];
	[super dealloc];
}

- (void)start
{
	// Asking without a usage description ends the process, and the editor's own
	// Info.plist has none for speech recognition: only a packaged build can listen.
	NSBundle* bundle = [NSBundle mainBundle];
	if ([bundle objectForInfoDictionaryKey:@"NSSpeechRecognitionUsageDescription"] == nil ||
		[bundle objectForInfoDictionaryKey:@"NSMicrophoneUsageDescription"] == nil)
	{
		self.status = @"Voice needs a packaged build";
		return;
	}
	[SFSpeechRecognizer requestAuthorization:^(SFSpeechRecognizerAuthorizationStatus speech) {
		if (speech != SFSpeechRecognizerAuthorizationStatusAuthorized)
		{
			self.status = @"Speech recognition is not allowed";
			return;
		}
		[AVAudioApplication requestRecordPermissionWithCompletionHandler:^(BOOL granted) {
			if (!granted)
			{
				self.status = @"The microphone is not allowed";
				return;
			}
			dispatch_async(dispatch_get_main_queue(), ^{
				[self begin];
			});
		}];
	}];
}

- (void)begin
{
#if PLATFORM_VISIONOS
	// The engine's audio session plays only; recording needs PlayAndRecord. Mixing with
	// others leaves any other app's sound alone.
	NSError* sessionError = nil;
	AVAudioSession* session = [AVAudioSession sharedInstance];
	if (![session setCategory:AVAudioSessionCategoryPlayAndRecord
						 mode:AVAudioSessionModeDefault
					  options:AVAudioSessionCategoryOptionMixWithOthers
						error:&sessionError] ||
		![session setActive:YES error:&sessionError])
	{
		self.status = [NSString stringWithFormat:@"No audio session: %@", sessionError.localizedDescription];
		return;
	}
#endif
	// The system's language; Apple's recogniser takes contextual strings in any.
	_recognizer = [[SFSpeechRecognizer alloc] init];
	if (_recognizer == nil || !_recognizer.isAvailable)
	{
		self.status = @"No speech recogniser for this language";
		return;
	}
	_engine = [[AVAudioEngine alloc] init];
	AVAudioInputNode* input = _engine.inputNode;
	AVAudioFormat* format = [input outputFormatForBus:0];
	if (format.sampleRate <= 0.0)
	{
		self.status = @"No microphone";
		return;
	}
	[input installTapOnBus:0
				bufferSize:1024
					format:format
					 block:^(AVAudioPCMBuffer* buffer, AVAudioTime* when) {
						 SFSpeechAudioBufferRecognitionRequest* request = nil;
						 @synchronized(self)
						 {
							 request = [[_request retain] autorelease];
						 }
						 [request appendAudioPCMBuffer:buffer];
					 }];
	[_engine prepare];
	NSError* engineError = nil;
	if (![_engine startAndReturnError:&engineError])
	{
		self.status = [NSString stringWithFormat:@"The microphone would not start: %@", engineError.localizedDescription];
		return;
	}
	@synchronized(self)
	{
		_running = YES;
	}
	self.status = [NSString stringWithFormat:@"Listening, %@, %@", _recognizer.locale.localeIdentifier,
		_recognizer.supportsOnDeviceRecognition ? @"on the device" : @"through Apple's servers"];
	[self restart];
}

- (void)restart
{
	@synchronized(self)
	{
		if (!_running)
		{
			return;
		}
		[_task cancel];
		[_task release];
		[_request release];
		_generation += 1;
		const NSInteger generation = _generation;
		SFSpeechAudioBufferRecognitionRequest* request = [[SFSpeechAudioBufferRecognitionRequest alloc] init];
		request.shouldReportPartialResults = YES;
		// On the device where it can be, so nothing said leaves it and there is no limit
		// on how long it listens.
		request.requiresOnDeviceRecognition = _recognizer.supportsOnDeviceRecognition;
		request.contextualStrings = _phrases;
		request.taskHint = SFSpeechRecognitionTaskHintSearch;
		_request = request;
		_task = [[_recognizer recognitionTaskWithRequest:request
										   resultHandler:^(SFSpeechRecognitionResult* result, NSError* error) {
											   [self heard:result error:error generation:generation];
										   }] retain];
	}
}

- (void)heard:(SFSpeechRecognitionResult*)result error:(NSError*)error generation:(NSInteger)generation
{
	@synchronized(self)
	{
		if (generation != _generation)
		{
			return;
		}
		NSString* text = result != nil ? result.bestTranscription.formattedString : @"";
		const BOOL final = result.isFinal || error != nil;
		[_pending addObject:@[text, @(final), error != nil ? error.localizedDescription : @""]];
	}
}

- (void)finish
{
	@synchronized(self)
	{
		[_request endAudio];
	}
}

- (void)stop
{
	@synchronized(self)
	{
		_running = NO;
		_generation += 1;
		[_task cancel];
		[_task release];
		_task = nil;
		[_request release];
		_request = nil;
	}
	if (_engine != nil)
	{
		[_engine.inputNode removeTapOnBus:0];
		[_engine stop];
	}
}

- (NSArray<NSArray*>*)drain
{
	@synchronized(self)
	{
		NSArray<NSArray*>* out = [[_pending copy] autorelease];
		[_pending removeAllObjects];
		return out;
	}
}
@end

namespace
{
	FString ToString(NSString* Text)
	{
		return Text != nil ? FString(UTF8_TO_TCHAR([Text UTF8String])) : FString();
	}

	class FAppleVoiceRecognizer final : public FVoiceRecognizer
	{
	public:
		virtual ~FAppleVoiceRecognizer() override
		{
			[Listener stop];
			[Listener release];
		}

		virtual void Start(const TArray<FString>& Phrases) override
		{
			@autoreleasepool
			{
				NSMutableArray<NSString*>* List = [NSMutableArray arrayWithCapacity:Phrases.Num()];
				for (const FString& Phrase : Phrases)
				{
					[List addObject:Phrase.GetNSString()];
				}
				Listener = [[StellarExplorerListener alloc] initWithPhrases:List];
				[Listener start];
			}
		}

		virtual void Restart() override
		{
			[Listener restart];
		}

		virtual void Finish() override
		{
			[Listener finish];
		}

		virtual bool Poll(FHeard& Out) override
		{
			if (Queue.IsEmpty())
			{
				@autoreleasepool
				{
					for (NSArray* Item in [Listener drain])
					{
						FHeard Heard;
						Heard.Text = ToString(Item[0]);
						Heard.bFinal = [Item[1] boolValue];
						Heard.Error = ToString(Item[2]);
						Queue.Add(MoveTemp(Heard));
					}
				}
			}
			if (Queue.IsEmpty())
			{
				return false;
			}
			Out = Queue[0];
			Queue.RemoveAt(0);
			return true;
		}

		virtual FString GetStatus() const override
		{
			@autoreleasepool
			{
				return Listener != nil ? ToString(Listener.status) : FString(TEXT("Not started"));
			}
		}

	private:
		StellarExplorerListener* Listener = nil;
		TArray<FHeard> Queue;
	};
}

TUniquePtr<FVoiceRecognizer> FVoiceRecognizer::Create()
{
	return MakeUnique<FAppleVoiceRecognizer>();
}

#else

TUniquePtr<FVoiceRecognizer> FVoiceRecognizer::Create()
{
	// Windows: to come (the platform's recogniser or Whisper; see the voice spike).
	return nullptr;
}

#endif
