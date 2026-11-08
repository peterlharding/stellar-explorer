#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "GameFramework/Actor.h"
#include "StarConsole.generated.h"

class AStarActions;
class AStarField;
class AStarPointer;
class FLogTap;
class FSocket;
class ITableRow;
class STableViewBase;
class STextBlock;
class SWidget;
template <typename ItemType>
class SListView;

/**
 * The console (F10 in the direction notes): a panel down the left of the desktop app's window,
 * with the view of the stars beside it. Tab hides and shows it.
 *
 *  - On its own the app steers itself: commands go to its own AStarActions, and the log is
 *    its own.
 *  - With -Console=<host>[:<port>] it steers the app on that machine instead, a headset's
 *    or another desktop's, through that app's control service (AStarControl), and this
 *    app's view can follow where the wearer is and looks. It reconnects by itself when the
 *    connection drops, as when the headset app restarts.
 *
 * Either way the panel shows the state (build, position, the star kept and the one pointed
 * at), a command box (typed, so never misheard), buttons for the commands that need no name,
 * the log (warnings amber, errors red, with a filter), and Quit; steering another app, also
 * "Quit the headset app", which quits that one.
 */
UCLASS()
class AStarConsole : public AActor
{
	GENERATED_BODY()

public:
	AStarConsole();

	/** Target empty: steer this app; else host[:port] to steer over the network. */
	void Setup(AStarActions* InActions, AStarPointer* InPointer, AStarField* InStarField, const FString& Target);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool IsRemote() const { return !Host.IsEmpty(); }
	void BuildPanel();
	void Connect();
	void Disconnect(const FString& Why);
	void Receive();
	void Handle(const FString& Line);
	void SendCommand(const FString& Command);
	void ShowResult(const FString& Command, const FString& Text);
	/** The state lines: Position in parsecs, ICRS. */
	void ShowState(const FString& Build, const TOptional<FVector>& Position, const FString& Kept, const FString& Pointed);
	void ShowLocalState();
	void AddLog(const FString& Line);
	/** Show only the log lines with every word of Filter in them; empty shows all. */
	void SetFilter(const FString& Filter);
	bool Matches(const FString& Line) const;
	void Follow(float DeltaSeconds);
	TSharedRef<ITableRow> MakeLogRow(TSharedPtr<FString> Line, const TSharedRef<STableViewBase>& OwnerTable) const;

	UPROPERTY()
	TObjectPtr<AStarActions> Actions;

	UPROPERTY()
	TObjectPtr<AStarPointer> Pointer;

	UPROPERTY()
	TObjectPtr<AStarField> StarField;

	/** Steering this app: its own log. */
	TSharedPtr<FLogTap> LogTap;
	uint64 LogTaken = 0;

	/** Steering another app: where, and the connection. */
	FString Host;
	int32 Port = 47800;
	FSocket* Socket = nullptr;
	TFuture<FSocket*> Connecting;
	double NextAttempt = 0.0;
	/** The other app has answered on this connection: "connected" means that, not just accepted. */
	bool bAnswered = false;
	/** "quit" was the last command sent: the app closing next is its doing, not a fault. */
	bool bQuitSent = false;
	/** Since the connection was lost, what the panel says happened; empty before, and once back. */
	FString Lost;
	TArray<uint8> In;

	/** The window's title: what this app is, and its build. */
	FText Title;
	TSharedPtr<SWidget> Panel;
	bool bPanelShown = true;
	bool bTabWasDown = false;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> StateText;
	TSharedPtr<STextBlock> ResultText;
	TSharedPtr<SListView<TSharedPtr<FString>>> LogView;
	TArray<TSharedPtr<FString>> LogLines;
	/** The lines the filter lets through, shown in the list. */
	TArray<TSharedPtr<FString>> ShownLines;
	TArray<FString> FilterWords;
	bool bLogChanged = false;
	double NextState = 0.0;

	bool bFollow = true;
	/**
	 * The view radius slider's place (AStarField::ViewFraction), from the app it steers except
	 * while held; and when it last sent the radius, which it does at most every SliderSendSeconds.
	 */
	double SliderValue = 1.0;
	bool bSliderHeld = false;
	double NextSliderSend = 0.0;
	/** Send the slider's radius as a command: "show within ... parsecs", or "show everything". */
	void SendViewRadius();
	/** The other app's head, in this app's frame, once a state has come. */
	TOptional<FVector> HeadsetPosition;
	FVector HeadsetFacing = FVector::ForwardVector;
	/** Following is paused until then, while the mouse is in use. */
	double FollowAfter = 0.0;
};
