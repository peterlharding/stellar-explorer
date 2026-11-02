#include "StarControl.h"

#include "BuildStamp.h"
#include "LogTap.h"
#include "Camera/PlayerCameraManager.h"
#include "Common/TcpListener.h"
#include "Dom/JsonObject.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "StarActions.h"
#include "StarField.h"
#include "StarPointer.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarControl, Log, All);

namespace
{
	constexpr double StateSeconds = 0.2;
	/** How long a console's command result stays above the panel. */
	constexpr double ResultSeconds = 6.0;
	/** A console that falls this far behind is dropped rather than left to fill memory. */
	constexpr int32 MaxPendingBytes = 4 * 1024 * 1024;

	/** An Unreal-frame vector as ICRS: parsecs for a position (Scale), y flipped back. */
	TArray<TSharedPtr<FJsonValue>> Icrs(const FVector& V, double Scale)
	{
		return {MakeShared<FJsonValueNumber>(V.X / Scale), MakeShared<FJsonValueNumber>(-V.Y / Scale), MakeShared<FJsonValueNumber>(V.Z / Scale)};
	}
}

AStarControl::AStarControl()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AStarControl::Setup(AStarActions* InActions, AStarPointer* InPointer, AStarField* InStarField)
{
	Actions = InActions;
	Pointer = InPointer;
	StarField = InStarField;
}

void AStarControl::BeginPlay()
{
	Super::BeginPlay();
	FParse::Value(FCommandLine::Get(), TEXT("ControlPort="), ControlPort);
	if (FParse::Param(FCommandLine::Get(), TEXT("NoControl")) || ControlPort <= 0)
	{
		UE_LOG(LogStarControl, Display, TEXT("Control service off"));
		return;
	}
	LogTap = MakeShared<FLogTap>();
	GLog->AddOutputDevice(LogTap.Get());
	// Not reusable: a port already taken (by another instance, or a process that inherited
	// its socket) must fail here, not leave two listeners where consoles reach the wrong one.
	Listener = MakeShared<FTcpListener>(FIPv4Endpoint(FIPv4Address::Any, static_cast<uint16>(ControlPort)), FTimespan::FromMilliseconds(100), false);
	Listener->OnConnectionAccepted().BindLambda([this](FSocket* Socket, const FIPv4Endpoint& From) {
		Accepted.Enqueue(TPair<FSocket*, FString>(Socket, From.ToString()));
		return true;
	});
	if (!Listener->IsActive())
	{
		UE_LOG(LogStarControl, Warning, TEXT("Control service cannot listen on port %d: taken by another process?"), ControlPort);
		Listener.Reset();
		return;
	}
	UE_LOG(LogStarControl, Display, TEXT("Control service listening on port %d"), ControlPort);
}

void AStarControl::EndPlay(const EEndPlayReason::Type Reason)
{
	Listener.Reset();
	for (FClient& Client : Clients)
	{
		Close(Client);
	}
	Clients.Reset();
	TPair<FSocket*, FString> Pending;
	while (Accepted.Dequeue(Pending))
	{
		FClient Client;
		Client.Socket = Pending.Key;
		Close(Client);
	}
	if (LogTap)
	{
		GLog->RemoveOutputDevice(LogTap.Get());
		LogTap.Reset();
	}
	Super::EndPlay(Reason);
}

void AStarControl::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Listener)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();

	// New consoles get the recent log first, then everything from now on.
	TPair<FSocket*, FString> New;
	while (Accepted.Dequeue(New))
	{
		FClient& Client = Clients.AddDefaulted_GetRef();
		Client.Socket = New.Key;
		Client.Address = New.Value;
		Client.Socket->SetNonBlocking(true);
		UE_LOG(LogStarControl, Display, TEXT("Console connected from %s"), *Client.Address);
	}
	if (Clients.IsEmpty())
	{
		return;
	}
	for (FClient& Client : Clients)
	{
		for (const FString& Line : LogTap->Since(Client.LogSent))
		{
			const TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
			Message->SetStringField(TEXT("type"), TEXT("log"));
			Message->SetStringField(TEXT("line"), Line);
			Send(Client, Message);
		}
	}
	if (Now >= NextState)
	{
		NextState = Now + StateSeconds;
		SendAll(State());
	}

	for (int32 Index = Clients.Num() - 1; Index >= 0; --Index)
	{
		FClient& Client = Clients[Index];
		if (!Receive(Client) || !Flush(Client) || Client.Out.Num() > MaxPendingBytes)
		{
			UE_LOG(LogStarControl, Display, TEXT("Console at %s disconnected"), *Client.Address);
			Close(Client);
			Clients.RemoveAt(Index);
		}
	}

	if (LineUntil > 0.0 && Now > LineUntil)
	{
		Pointer->SetVoiceLine(FString(), false);
		LineUntil = 0.0;
	}
}

bool AStarControl::Receive(FClient& Client)
{
	uint32 Size = 0;
	while (Client.Socket->HasPendingData(Size) && Size > 0)
	{
		const int32 Start = Client.In.Num();
		Client.In.AddUninitialized(static_cast<int32>(Size));
		int32 Read = 0;
		if (!Client.Socket->Recv(Client.In.GetData() + Start, static_cast<int32>(Size), Read))
		{
			return false;
		}
		Client.In.SetNum(Start + Read, EAllowShrinking::No);
	}
	if (Client.Socket->GetConnectionState() != SCS_Connected)
	{
		return false;
	}

	int32 LineEnd = INDEX_NONE;
	while (Client.In.Find(static_cast<uint8>('\n'), LineEnd))
	{
		const FString Line = FString(FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(Client.In.GetData()), LineEnd)).TrimStartAndEnd();
		Client.In.RemoveAt(0, LineEnd + 1, EAllowShrinking::No);
		TSharedPtr<FJsonObject> Message;
		FString Command;
		if (Line.IsEmpty() || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Message) || !Message.IsValid() ||
			!Message->TryGetStringField(TEXT("command"), Command))
		{
			UE_LOG(LogStarControl, Warning, TEXT("Console sent what is not a command: %s"), *Line.Left(200));
			continue;
		}
		const FString Result = Actions->Run(Command);
		UE_LOG(LogStarControl, Display, TEXT("Console: \"%s\": %s"), *Command, *Result);
		Pointer->SetVoiceLine(Result, false);
		LineUntil = FPlatformTime::Seconds() + ResultSeconds;
		const TSharedRef<FJsonObject> Reply = MakeShared<FJsonObject>();
		Reply->SetStringField(TEXT("type"), TEXT("result"));
		Reply->SetStringField(TEXT("command"), Command);
		Reply->SetStringField(TEXT("text"), Result);
		Send(Client, Reply);
	}
	return true;
}

bool AStarControl::Flush(FClient& Client)
{
	while (Client.Out.Num() > 0)
	{
		int32 Sent = 0;
		if (!Client.Socket->Send(Client.Out.GetData(), Client.Out.Num(), Sent))
		{
			// A full socket buffer is not a disconnection: try again next frame.
			return ISocketSubsystem::Get()->GetLastErrorCode() == SE_EWOULDBLOCK;
		}
		if (Sent <= 0)
		{
			return true;
		}
		Client.Out.RemoveAt(0, Sent, EAllowShrinking::No);
	}
	return true;
}

void AStarControl::Send(FClient& Client, const TSharedRef<FJsonObject>& Message) const
{
	FString Text;
	FJsonSerializer::Serialize(Message, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
	Text += TEXT("\n");
	const FTCHARToUTF8 Utf8(*Text);
	Client.Out.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}

void AStarControl::SendAll(const TSharedRef<FJsonObject>& Message)
{
	for (FClient& Client : Clients)
	{
		Send(Client, Message);
	}
}

void AStarControl::Close(FClient& Client) const
{
	if (Client.Socket != nullptr)
	{
		Client.Socket->Close();
		ISocketSubsystem::Get()->DestroySocket(Client.Socket);
		Client.Socket = nullptr;
	}
}

TSharedRef<FJsonObject> AStarControl::State() const
{
	const TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
	Message->SetStringField(TEXT("type"), TEXT("state"));
	Message->SetStringField(TEXT("build"), FString::Printf(TEXT("%s on %s, built %s"), STELLAR_SPIKE_COMMIT, STELLAR_SPIKE_BRANCH, STELLAR_SPIKE_BUILT));
	if (const APlayerController* Player = GetWorld()->GetFirstPlayerController(); Player != nullptr && Player->PlayerCameraManager != nullptr)
	{
		Message->SetArrayField(TEXT("position"), Icrs(Player->PlayerCameraManager->GetCameraLocation(), AStarField::UnitsPerParsec));
		Message->SetArrayField(TEXT("facing"), Icrs(Player->PlayerCameraManager->GetCameraRotation().Vector(), 1.0));
		Message->SetArrayField(TEXT("up"), Icrs(FRotationMatrix(Player->PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Z), 1.0));
	}
	const int32 Kept = Pointer->GetKept();
	const int32 Pointed = Pointer->GetPointed();
	Message->SetStringField(TEXT("kept"), Kept != INDEX_NONE ? StarField->GetName(Kept) : FString());
	if (Kept != INDEX_NONE)
	{
		Message->SetArrayField(TEXT("kept_position"), Icrs(StarField->GetPosition(Kept), AStarField::UnitsPerParsec));
	}
	Message->SetStringField(TEXT("pointed"), Pointed != INDEX_NONE ? StarField->GetName(Pointed) : FString());
	return Message;
}
