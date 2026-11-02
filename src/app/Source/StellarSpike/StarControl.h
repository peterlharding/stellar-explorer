#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "GameFramework/Actor.h"
#include "StarControl.generated.h"

class AStarActions;
class AStarField;
class AStarPointer;
class FSocket;
class FTcpListener;
class FLogTap;

/**
 * The control service: what the desktop console (F10 in the direction notes) talks to. It
 * listens on ControlPort and exchanges one JSON object per line with each console connected:
 *
 *   from the console  {"command": "go to Sirius"}     run through AStarActions, as if spoken
 *   to the console    {"type": "result", "command": ..., "text": ...}
 *                     {"type": "state", "position": [x, y, z], "facing": [x, y, z], "up": [x, y, z],
 *                      "kept": ..., "kept_position": [x, y, z], "pointed": ..., "build": ...}
 *                      five times a second;
 *                      position in parsecs and facing a unit vector, both ICRS
 *                     {"type": "log", "line": ...}     every log line, the last 500 on connecting;
 *                      a warning or an error says so after its category ("LogX: Warning: ...")
 *
 * Off with -NoControl, or ControlPort=0.
 */
UCLASS(Config = Game)
class AStarControl : public AActor
{
	GENERATED_BODY()

public:
	AStarControl();

	void Setup(AStarActions* InActions, AStarPointer* InPointer, AStarField* InStarField);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FClient
	{
		FSocket* Socket = nullptr;
		FString Address;
		/** Received, not yet a whole line. */
		TArray<uint8> In;
		/** To send, not yet taken by the socket. */
		TArray<uint8> Out;
		/** The number of the last log line sent. */
		uint64 LogSent = 0;
	};

	void Send(FClient& Client, const TSharedRef<class FJsonObject>& Message) const;
	void SendAll(const TSharedRef<class FJsonObject>& Message);
	/** Receive and act on whole lines; false once the console has gone. */
	bool Receive(FClient& Client);
	/** Send what the socket will take now; false once the console has gone. */
	static bool Flush(FClient& Client);
	void Close(FClient& Client) const;
	TSharedRef<class FJsonObject> State() const;

	UPROPERTY()
	TObjectPtr<AStarActions> Actions;

	UPROPERTY()
	TObjectPtr<AStarPointer> Pointer;

	UPROPERTY()
	TObjectPtr<AStarField> StarField;

	/** The TCP port consoles connect to; 0 turns the service off. -ControlPort= overrides it. */
	UPROPERTY(Config)
	int32 ControlPort = 47800;

	// Shared, not unique, pointers: their deleters are made where the types are complete.
	TSharedPtr<FTcpListener> Listener;
	/** Accepted on the listener's thread, taken on the game thread. */
	TQueue<TPair<FSocket*, FString>, EQueueMode::Mpsc> Accepted;
	TArray<FClient> Clients;
	TSharedPtr<FLogTap> LogTap;
	double NextState = 0.0;
	double LineUntil = 0.0;
};
