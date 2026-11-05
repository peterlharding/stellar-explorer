#include "StarConsole.h"

#include "Async/Async.h"
#include "BuildStamp.h"
#include "Camera/PlayerCameraManager.h"
#include "DesktopMouse.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LogTap.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "StarActions.h"
#include "StarField.h"
#include "StarPointer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarConsole, Log, All);

namespace
{
	constexpr double RetrySeconds = 2.0;
	/**
	 * A connection attempt gives up after this. Left to the system, one to a headset that has
	 * gone (asleep, or its app quit) took more than a minute, and quitting the console waited
	 * for it.
	 */
	constexpr double ConnectSeconds = 2.0;
	/** Why Receive disconnects when the other app closes its end. */
	const TCHAR* const ClosedByApp = TEXT("the other app closed the connection");
	constexpr double StateSeconds = 0.2;
	/** Lines of the log kept in the panel. */
	constexpr int32 MaxLogLines = 5000;
	/** How quickly the view catches up with the other app's, following it. */
	constexpr double FollowSeconds = 0.15;
	/** Following pauses while a mouse button is down, and this long after. */
	constexpr double FollowPauseSeconds = 2.0;
	constexpr float PanelWidth = 420.0f;
	const FLinearColor PanelColour(0.03f, 0.035f, 0.05f, 0.92f);

	/** The commands that need no star's name, as buttons. */
	const TCHAR* const Buttons[] = {TEXT("Go there"), TEXT("Back off"), TEXT("Closer"), TEXT("Let go"), TEXT("Return to the Sun"),
		TEXT("Galactic north"), TEXT("Celestial north"), TEXT("Hide info"), TEXT("Show info"), TEXT("Show more"), TEXT("Show less"), TEXT("Show everything"), TEXT("Backtrack")};
	/** While the view radius slider is dragged, it sends no more often than this. */
	constexpr double SliderSendSeconds = 0.1;

	/** An ICRS vector from the control service in this app's frame (y flipped), scaled. */
	bool ReadIcrs(const TSharedPtr<FJsonObject>& Message, const TCHAR* Field, double Scale, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Message->TryGetArrayField(Field, Values) || Values->Num() != 3)
		{
			return false;
		}
		Out = FVector((*Values)[0]->AsNumber() * Scale, -(*Values)[1]->AsNumber() * Scale, (*Values)[2]->AsNumber() * Scale);
		return true;
	}

	/** Warnings amber and errors red, as FLogTap marks them; the rest plain. */
	FSlateColor LineColour(const FString& Line)
	{
		if (Line.Contains(TEXT(": Error: ")))
		{
			return FSlateColor(FLinearColor(1.0f, 0.35f, 0.3f));
		}
		if (Line.Contains(TEXT(": Warning: ")))
		{
			return FSlateColor(FLinearColor(1.0f, 0.75f, 0.25f));
		}
		return FSlateColor::UseForeground();
	}

	FString Name(const AStarField* StarField, int32 Star)
	{
		return Star != INDEX_NONE ? StarField->GetName(Star) : FString();
	}
}

AStarConsole::AStarConsole()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AStarConsole::Setup(AStarActions* InActions, AStarPointer* InPointer, AStarField* InStarField, const FString& Target)
{
	Actions = InActions;
	Pointer = InPointer;
	StarField = InStarField;
	FString PortText;
	if (Target.Split(TEXT(":"), &Host, &PortText, ESearchCase::IgnoreCase, ESearchDir::FromEnd) && PortText.IsNumeric())
	{
		Port = FCString::Atoi(*PortText);
	}
	else
	{
		Host = Target;
	}
}

void AStarConsole::BeginPlay()
{
	Super::BeginPlay();
	if (IsRemote())
	{
		UE_LOG(LogStarConsole, Display, TEXT("Console steering the app on %s:%d"), *Host, Port);
	}
	else
	{
		LogTap = MakeShared<FLogTap>();
		GLog->AddOutputDevice(LogTap.Get());
	}
	BuildPanel();
	// The window's close button quits as the Quit button does. Left to destroy the window
	// itself, it did so off the game thread and the engine stopped on an assertion
	// (IsInGameThread, SceneViewport.cpp), every time.
	if (GEngine->GameViewport != nullptr)
	{
		GEngine->GameViewport->OnWindowCloseRequested().BindLambda([]() {
			FPlatformMisc::RequestExit(false, TEXT("Window closed"));
			return false;
		});
	}
}

void AStarConsole::EndPlay(const EEndPlayReason::Type Reason)
{
	Disconnect(TEXT("closing"));
	if (Connecting.IsValid())
	{
		if (FSocket* Late = Connecting.Get())
		{
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Late);
		}
	}
	if (LogTap.IsValid())
	{
		GLog->RemoveOutputDevice(LogTap.Get());
		LogTap.Reset();
	}
	if (GEngine->GameViewport != nullptr)
	{
		GEngine->GameViewport->OnWindowCloseRequested().Unbind();
		if (Panel.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
		}
	}
	Super::EndPlay(Reason);
}

void AStarConsole::BuildPanel()
{
	if (GEngine->GameViewport == nullptr)
	{
		return;
	}
	const TWeakObjectPtr<AStarConsole> Weak(this);
	TSharedRef<SWrapBox> ButtonRow = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.0, 6.0));
	for (const TCHAR* Label : Buttons)
	{
		ButtonRow->AddSlot()[SNew(SButton).Text(FText::FromString(Label)).OnClicked_Lambda([Weak, Command = FString(Label)]() {
			if (Weak.IsValid())
			{
				Weak->SendCommand(Command);
			}
			return FReply::Handled();
		})];
	}

	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
	Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[SNew(STextBlock)
				.Text(FText::FromString(IsRemote() ? FString::Printf(TEXT("Console: steering %s"), *Host) : TEXT("Console: this app")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))];
	Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[SAssignNew(StatusText, STextBlock).AutoWrapText(true).Text(FText::FromString(IsRemote() ? TEXT("Connecting...") : TEXT("Tab hides this panel")))];
	Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[SAssignNew(StateText, STextBlock).AutoWrapText(true)];
	Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[SNew(SEditableTextBox)
				.HintText(FText::FromString(TEXT("A command, then Enter: go to Vega")))
				.ClearKeyboardFocusOnCommit(false)
				.SelectAllTextOnCommit(true)
				.OnTextCommitted_Lambda([Weak](const FText& Text, ETextCommit::Type How) {
					if (How == ETextCommit::OnEnter && Weak.IsValid() && !Text.IsEmpty())
					{
						Weak->SendCommand(Text.ToString());
					}
				})];
	Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[ButtonRow];
	// How far out stars are shown, evenly by ratio, every star at the right-hand end.
	Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[SNew(STextBlock).Text_Lambda([Weak]() {
			return FText::FromString(Weak.IsValid() ? AStarField::DescribeViewRadius(AStarField::ViewRadiusAt(Weak->SliderValue)) : FString());
		})];
	Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[SNew(SSlider)
				.Value_Lambda([Weak]() { return Weak.IsValid() ? static_cast<float>(Weak->SliderValue) : 1.0f; })
				.OnMouseCaptureBegin_Lambda([Weak]() {
					if (Weak.IsValid())
					{
						Weak->bSliderHeld = true;
					}
				})
				.OnMouseCaptureEnd_Lambda([Weak]() {
					if (Weak.IsValid())
					{
						Weak->bSliderHeld = false;
						Weak->SendViewRadius();
					}
				})
				.OnValueChanged_Lambda([Weak](float Value) {
					if (Weak.IsValid())
					{
						Weak->SliderValue = Value;
						if (FPlatformTime::Seconds() >= Weak->NextSliderSend)
						{
							Weak->SendViewRadius();
						}
					}
				})];
	if (IsRemote())
	{
		Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[SNew(SCheckBox)
					.IsChecked_Lambda([Weak]() { return Weak.IsValid() && Weak->bFollow ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([Weak](ECheckBoxState State) {
						if (Weak.IsValid())
						{
							Weak->bFollow = State == ECheckBoxState::Checked;
						}
					})[SNew(STextBlock).Text(FText::FromString(TEXT("Follow its view")))]];
	}
	Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[SAssignNew(ResultText, STextBlock).AutoWrapText(true)];
	Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[SNew(SEditableTextBox)
				.HintText(FText::FromString(TEXT("Filter the log: StarVoice heard")))
				.OnTextChanged_Lambda([Weak](const FText& Text) {
					if (Weak.IsValid())
					{
						Weak->SetFilter(Text.ToString());
					}
				})];
	Column->AddSlot().FillHeight(1.0f).Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[SAssignNew(LogView, SListView<TSharedPtr<FString>>)
				.ListItemsSource(&ShownLines)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow_Lambda([Weak](TSharedPtr<FString> Line, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow> {
					return Weak.IsValid() ? Weak->MakeLogRow(Line, OwnerTable) : SNew(STableRow<TSharedPtr<FString>>, OwnerTable);
				})];
	// Quit this app as a menu or Command-Q would, so it shuts down cleanly; steering another,
	// that one too, by the control service's "quit".
	TSharedRef<SHorizontalBox> QuitRow = SNew(SHorizontalBox);
	if (IsRemote())
	{
		QuitRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[SNew(SButton).Text(FText::FromString(TEXT("Quit the headset app"))).OnClicked_Lambda([Weak]() {
				if (Weak.IsValid())
				{
					Weak->SendCommand(TEXT("quit"));
				}
				return FReply::Handled();
			})];
	}
	QuitRow->AddSlot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Quit"))).OnClicked_Lambda([]() {
		FPlatformMisc::RequestExit(false, TEXT("Console"));
		return FReply::Handled();
	})];
	Column->AddSlot().AutoHeight().HAlign(HAlign_Right)[QuitRow];

	// The panel down the left; the rest of the window is the view, which the box lets clicks through to.
	Panel = SNew(SHorizontalBox).Visibility(EVisibility::SelfHitTestInvisible)
		+ SHorizontalBox::Slot().AutoWidth()
			[SNew(SBox).WidthOverride(PanelWidth)
					[SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
							.BorderBackgroundColor(PanelColour)
							.Padding(12.0f)[Column]]];
	GEngine->GameViewport->AddViewportWidgetContent(Panel.ToSharedRef(), 10);
	const FString RolePrefix = IsRemote() ? FString::Printf(TEXT("Stellar Explorer: console for %s"), *Host) : FString(TEXT("Stellar Explorer"));
	Title = FText::FromString(FString::Printf(TEXT("%s %s (build %s, %s)"), *RolePrefix, *StellarVersion(), STELLAR_SPIKE_COMMIT, STELLAR_SPIKE_BRANCH));
	FDesktopMouse::SetExcluded(Panel);
}

TSharedRef<ITableRow> AStarConsole::MakeLogRow(TSharedPtr<FString> Line, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[SNew(STextBlock).Text(FText::FromString(*Line)).ColorAndOpacity(LineColour(*Line)).Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))];
}

void AStarConsole::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = FPlatformTime::Seconds();

	// The window says what it is, in place of the engine's "StellarSpike (64-bit Development
	// ...)". Set as it differs: the engine sets its own after the panel is built.
	if (GEngine->GameViewport != nullptr)
	{
		if (const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow(); Window.IsValid() && !Window->GetTitle().EqualTo(Title))
		{
			Window->SetTitle(Title);
		}
	}

	// Tab shows and hides the panel (not while typing in it, where Slate has the key).
	if (const APlayerController* Player = GetWorld()->GetFirstPlayerController(); Player != nullptr && Panel.IsValid())
	{
		const bool bTabDown = Player->IsInputKeyDown(EKeys::Tab);
		if (bTabDown && !bTabWasDown)
		{
			bPanelShown = !bPanelShown;
			Panel->SetVisibility(bPanelShown ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
		}
		bTabWasDown = bTabDown;
	}

	if (!IsRemote())
	{
		for (const FString& Line : LogTap->Since(LogTaken))
		{
			AddLog(Line);
		}
		if (Now >= NextState)
		{
			NextState = Now + StateSeconds;
			ShowLocalState();
		}
	}
	else if (Socket == nullptr)
	{
		if (Connecting.IsValid() && Connecting.IsReady())
		{
			Socket = Connecting.Get();
			Connecting.Reset();
			if (Socket != nullptr)
			{
				Socket->SetNonBlocking(true);
				In.Reset();
				LogLines.Reset();
				ShownLines.Reset();
				bLogChanged = true;
				bAnswered = false;
				if (Lost.IsEmpty())
				{
					StatusText->SetText(FText::FromString(FString::Printf(TEXT("Waiting for an answer from %s:%d..."), *Host, Port)));
				}
			}
			else
			{
				if (Lost.IsEmpty())
				{
					StatusText->SetText(FText::FromString(FString::Printf(TEXT("Cannot reach %s:%d; retrying"), *Host, Port)));
				}
				NextAttempt = Now + RetrySeconds;
			}
		}
		else if (!Connecting.IsValid() && Now >= NextAttempt)
		{
			Connect();
		}
	}
	if (IsRemote() && Socket != nullptr)
	{
		Receive();
	}

	if (bLogChanged && LogView.IsValid())
	{
		// Follow the newest line only when already at the bottom, so reading back is not
		// interrupted by lines arriving.
		const bool bAtBottom = LogView->GetScrollDistanceRemaining().Y <= 1.0;
		bLogChanged = false;
		LogView->RequestListRefresh();
		if (bAtBottom)
		{
			LogView->ScrollToBottom();
		}
	}
	if (IsRemote() && bFollow)
	{
		Follow(DeltaSeconds);
	}
}

void AStarConsole::Connect()
{
	if (Lost.IsEmpty())
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("Connecting to %s:%d..."), *Host, Port)));
	}
	// Name lookup and connecting block: off the game thread.
	Connecting = Async(EAsyncExecution::Thread, [Host = Host, Port = Port]() -> FSocket* {
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		// IPv4 first: the control service listens on IPv4, and asked for any address the
		// Mac's resolver gave only IPv6 link-local ones for a headset's .local name.
		TArray<FAddressInfoResultData> Results =
			Sockets->GetAddressInfo(*Host, nullptr, EAddressInfoFlags::Default, FNetworkProtocolTypes::IPv4, ESocketType::SOCKTYPE_Streaming).Results;
		Results.Append(Sockets->GetAddressInfo(*Host, nullptr, EAddressInfoFlags::Default, NAME_None, ESocketType::SOCKTYPE_Streaming).Results);
		for (const FAddressInfoResultData& Result : Results)
		{
			TSharedRef<FInternetAddr> Address = Result.Address->Clone();
			Address->SetPort(Port);
			FSocket* Candidate = Sockets->CreateSocket(NAME_Stream, TEXT("Stellar console"), Address->GetProtocolType());
			// Without blocking, so the attempt can be given up after ConnectSeconds.
			if (Candidate != nullptr && Candidate->SetNonBlocking(true))
			{
				Candidate->Connect(*Address);
				if (Candidate->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::FromSeconds(ConnectSeconds)) &&
					Candidate->GetConnectionState() == SCS_Connected)
				{
					return Candidate;
				}
			}
			if (Candidate != nullptr)
			{
				Sockets->DestroySocket(Candidate);
			}
		}
		return nullptr;
	});
}

void AStarConsole::Disconnect(const FString& Why)
{
	if (Socket == nullptr)
	{
		return;
	}
	Socket->Close();
	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
	Socket = nullptr;
	NextAttempt = FPlatformTime::Seconds() + RetrySeconds;
	UE_LOG(LogStarConsole, Display, TEXT("Disconnected from %s:%d: %s"), *Host, Port, *Why);
	// Said as what happened, and kept while the console waits for the app to start again.
	if (bQuitSent)
	{
		Lost = TEXT("The headset app has quit. Waiting for it to start again.");
	}
	else if (bAnswered && Why == ClosedByApp)
	{
		Lost = TEXT("The headset app has closed (quit, or the headset was taken off). Waiting for it to start again.");
	}
	else
	{
		Lost = FString::Printf(TEXT("Lost the connection to %s (%s). Trying again."), *Host, *Why);
	}
	bQuitSent = false;
	bAnswered = false;
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::FromString(Lost));
	}
}

void AStarConsole::Receive()
{
	uint32 Size = 0;
	while (Socket->HasPendingData(Size) && Size > 0)
	{
		const int32 Start = In.Num();
		In.AddUninitialized(static_cast<int32>(Size));
		int32 Read = 0;
		if (!Socket->Recv(In.GetData() + Start, static_cast<int32>(Size), Read))
		{
			Disconnect(TEXT("the connection failed"));
			return;
		}
		In.SetNum(Start + Read, EAllowShrinking::No);
	}
	if (Socket->GetConnectionState() != SCS_Connected)
	{
		Disconnect(ClosedByApp);
		return;
	}
	int32 LineEnd = INDEX_NONE;
	while (In.Find(static_cast<uint8>('\n'), LineEnd))
	{
		if (!bAnswered)
		{
			bAnswered = true;
			Lost.Reset();
			UE_LOG(LogStarConsole, Display, TEXT("Connected to %s:%d"), *Host, Port);
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("Connected to %s:%d. Tab hides this panel"), *Host, Port)));
		}
		Handle(FString(FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(In.GetData()), LineEnd)));
		In.RemoveAt(0, LineEnd + 1, EAllowShrinking::No);
	}
}

void AStarConsole::Handle(const FString& Line)
{
	TSharedPtr<FJsonObject> Message;
	FString Type;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Message) || !Message.IsValid() || !Message->TryGetStringField(TEXT("type"), Type))
	{
		return;
	}
	if (Type == TEXT("log"))
	{
		AddLog(Message->GetStringField(TEXT("line")));
	}
	else if (Type == TEXT("result"))
	{
		ShowResult(Message->GetStringField(TEXT("command")), Message->GetStringField(TEXT("text")));
	}
	else if (Type == TEXT("state"))
	{
		FVector Position;
		FVector Facing;
		TOptional<FVector> Icrs;
		if (ReadIcrs(Message, TEXT("position"), AStarField::UnitsPerParsec, Position) && ReadIcrs(Message, TEXT("facing"), 1.0, Facing))
		{
			HeadsetPosition = Position;
			HeadsetFacing = Facing.GetSafeNormal();
			Icrs = FVector(Position.X, -Position.Y, Position.Z) / AStarField::UnitsPerParsec;
		}
		ShowState(Message->GetStringField(TEXT("build")), Icrs, Message->GetStringField(TEXT("kept")), Message->GetStringField(TEXT("pointed")));
		// The same stars shown here as there, and the slider where it is set there.
		double Radius = 0.0;
		if (Message->TryGetNumberField(TEXT("view_radius"), Radius))
		{
			StarField->SetViewRadius(Radius);
			if (!bSliderHeld)
			{
				SliderValue = AStarField::ViewFraction(Radius);
			}
		}
	}
}

void AStarConsole::SendViewRadius()
{
	NextSliderSend = FPlatformTime::Seconds() + SliderSendSeconds;
	const double Radius = AStarField::ViewRadiusAt(SliderValue);
	// Four decimals: a point written with fewer than three might be read as a thousands separator.
	SendCommand(Radius > 0.0 ? FString::Printf(TEXT("show within %.4f parsecs"), Radius) : FString(TEXT("show everything")));
}

void AStarConsole::SendCommand(const FString& Command)
{
	if (!IsRemote())
	{
		ShowResult(Command, Actions->Run(Command));
		return;
	}
	if (Socket == nullptr || !bAnswered)
	{
		ShowResult(Command, Lost.IsEmpty() ? FString::Printf(TEXT("not connected to %s yet"), *Host) : Lost);
		return;
	}
	const TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
	Message->SetStringField(TEXT("command"), Command);
	FString Text;
	FJsonSerializer::Serialize(Message, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
	Text += TEXT("\n");
	const FTCHARToUTF8 Utf8(*Text);
	// Commands are a line each, far smaller than any socket buffer: one send is enough.
	int32 Sent = 0;
	if (!Socket->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent) || Sent != Utf8.Length())
	{
		Disconnect(TEXT("sending failed"));
		return;
	}
	bQuitSent = Command.Equals(TEXT("quit"), ESearchCase::IgnoreCase);
	ShowResult(Command, TEXT("sent..."));
}

void AStarConsole::ShowResult(const FString& Command, const FString& Text)
{
	ResultText->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"), *Command, *Text)));
}

void AStarConsole::ShowState(const FString& Build, const TOptional<FVector>& Position, const FString& Kept, const FString& Pointed)
{
	FString State = Build;
	if (Position.IsSet())
	{
		// Rounded first, and +0.0 so that -0.001 shows as 0.00, not -0.00.
		const FVector P = Position.GetValue();
		const FVector R = FVector(FMath::RoundToDouble(P.X * 100.0), FMath::RoundToDouble(P.Y * 100.0), FMath::RoundToDouble(P.Z * 100.0)) / 100.0 + FVector(0.0);
		State += FString::Printf(TEXT("\nAt %.2f, %.2f, %.2f pc from the Sun"), R.X, R.Y, R.Z);
	}
	State += FString::Printf(TEXT("\nKept: %s\nPointed at: %s"), Kept.IsEmpty() ? TEXT("none") : *Kept, Pointed.IsEmpty() ? TEXT("none") : *Pointed);
	StateText->SetText(FText::FromString(State));
}

void AStarConsole::ShowLocalState()
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	TOptional<FVector> Position;
	if (Player != nullptr && Player->PlayerCameraManager != nullptr)
	{
		const FVector Head = Player->PlayerCameraManager->GetCameraLocation() / AStarField::UnitsPerParsec;
		Position = FVector(Head.X, -Head.Y, Head.Z);
	}
	ShowState(StellarBuild(), Position,
		Name(StarField, Pointer->GetKept()), Name(StarField, Pointer->GetPointed()));
	if (!bSliderHeld)
	{
		SliderValue = AStarField::ViewFraction(StarField->GetViewRadius());
	}
}

void AStarConsole::AddLog(const FString& Line)
{
	const TSharedPtr<FString> Added = MakeShared<FString>(Line);
	LogLines.Add(Added);
	if (LogLines.Num() > MaxLogLines)
	{
		LogLines.RemoveAt(0, LogLines.Num() - MaxLogLines, EAllowShrinking::No);
	}
	if (Matches(*Added))
	{
		ShownLines.Add(Added);
		if (ShownLines.Num() > MaxLogLines)
		{
			ShownLines.RemoveAt(0, ShownLines.Num() - MaxLogLines, EAllowShrinking::No);
		}
	}
	bLogChanged = true;
}

bool AStarConsole::Matches(const FString& Line) const
{
	for (const FString& Word : FilterWords)
	{
		if (!Line.Contains(Word))
		{
			return false;
		}
	}
	return true;
}

void AStarConsole::SetFilter(const FString& Filter)
{
	Filter.ParseIntoArrayWS(FilterWords);
	ShownLines.Reset();
	for (const TSharedPtr<FString>& Line : LogLines)
	{
		if (Matches(*Line))
		{
			ShownLines.Add(Line);
		}
	}
	bLogChanged = true;
}

void AStarConsole::Follow(float DeltaSeconds)
{
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = Player != nullptr ? Player->GetPawn() : nullptr;
	if (Pawn == nullptr || !HeadsetPosition.IsSet())
	{
		return;
	}
	// Let the mouse have the view while it is being used, and for a moment after.
	const double Now = FPlatformTime::Seconds();
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetPressedMouseButtons().Num() > 0)
	{
		FollowAfter = Now + FollowPauseSeconds;
	}
	if (Now < FollowAfter)
	{
		return;
	}
	// Eased, since states come five times a second.
	const double Blend = 1.0 - FMath::Exp(-DeltaSeconds / FollowSeconds);
	Pawn->SetActorLocation(FMath::Lerp(Pawn->GetActorLocation(), HeadsetPosition.GetValue(), Blend));
	if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
	{
		Movement->StopMovementImmediately();
	}
	const FQuat Current = Player->GetControlRotation().Quaternion();
	Player->SetControlRotation(FQuat::Slerp(Current, HeadsetFacing.Rotation().Quaternion(), Blend).Rotator());
}
