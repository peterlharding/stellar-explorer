#include "StarActions.h"

#include "Async/Async.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "StarField.h"
#include "StarPointer.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarActions, Log, All);

namespace
{
	/**
	 * "Closer" stops this far from a star, twice the camera's near plane (NearClipPlane in
	 * DefaultEngine.ini, 0.01 pc), nearer than which it is not drawn.
	 */
	constexpr double NearestParsecs = 0.02;
	/** "Return to the Sun" stops this far from it, so it is in view: about 10,000 AU. */
	constexpr double SunViewParsecs = 0.05;
}

AStarActions::AStarActions()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AStarActions::Setup(AStarField* InStarField, AStarPointer* InPointer)
{
	StarField = InStarField;
	Pointer = InPointer;
}

void AStarActions::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Loading.IsValid())
	{
		Names = Loading.Get();
	}
	stc_voice_close(Names);
	Names = nullptr;
	Super::EndPlay(Reason);
}

void AStarActions::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (StarField == nullptr)
	{
		return;
	}
	if (!bLoadingStarted && !StarField->GetCataloguePath().IsEmpty())
	{
		bLoadingStarted = true;
		LoadStart = FPlatformTime::Seconds();
		// The names take a few tenths of a second to read and index: not on the game thread.
		Loading = Async(EAsyncExecution::Thread, [Path = StarField->GetCataloguePath()]() -> StcVoice* {
			StcCatalogue* Catalogue = stc_catalogue_open(TCHAR_TO_UTF8(*Path));
			StcVoice* Read = stc_voice_open(Catalogue);
			stc_catalogue_close(Catalogue);
			return Read;
		});
	}
	if (Loading.IsValid() && Loading.IsReady())
	{
		Names = Loading.Get();
		Loading.Reset();
		if (Names == nullptr)
		{
			UE_LOG(LogStarActions, Error, TEXT("Cannot read the star names from %s"), *StarField->GetCataloguePath());
		}
		else
		{
			UE_LOG(LogStarActions, Display, TEXT("%d star names read in %.2f s"), stc_voice_phrase_count(Names), FPlatformTime::Seconds() - LoadStart);
		}
	}
}

FString AStarActions::Run(const FString& Text)
{
	// The mouse's view moves, by name: "orbit 100 0" (pixels dragged), "zoom 2" (notches).
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	if (Words.Num() >= 2 && (Words[0] == TEXT("orbit") || Words[0] == TEXT("turn") || Words[0] == TEXT("pan") || Words[0] == TEXT("zoom")))
	{
		const FVector2D By(FCString::Atod(*Words[1]), Words.Num() > 2 ? FCString::Atod(*Words[2]) : 0.0);
		const FVector2D None = FVector2D::ZeroVector;
		Pointer->MoveView(Words[0] == TEXT("turn") ? By : None, Words[0] == TEXT("orbit") ? By : None, Words[0] == TEXT("pan") ? By : None,
			Words[0] == TEXT("zoom") ? static_cast<float>(By.X) : 0.0f);
		return FString::Printf(TEXT("%s %s"), *Words[0], *By.ToString());
	}
	if (Names == nullptr)
	{
		return TEXT("The star names are not read yet");
	}
	StcVoiceCommand Command;
	if (stc_voice_parse(Names, TCHAR_TO_UTF8(*Text), "", &Command) < 0)
	{
		return TEXT("Cannot parse that");
	}
	return Perform(Command, Text);
}

FString AStarActions::Perform(const StcVoiceCommand& Command, const FString& Heard)
{
	FString Name = UTF8_TO_TCHAR(Command.name);
	int32 Star = Command.star_id >= 0 ? StarField->FindById(Command.star_id) : INDEX_NONE;
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (Player == nullptr || Player->PlayerCameraManager == nullptr)
	{
		return TEXT("No viewer yet");
	}
	const FVector Head = Player->PlayerCameraManager->GetCameraLocation();
	switch (Command.action)
	{
	case STC_VOICE_GO_THERE:
		// No name to mishear: the star kept, or the one the ray points at.
		Star = Pointer->GetTarget();
		if (Star == INDEX_NONE)
		{
			return TEXT("Point at a star first");
		}
		Face(Star, GoNear(Star));
		Pointer->Keep(Star);
		return FString::Printf(TEXT("Going to %s"), *StarField->GetName(Star));
	case STC_VOICE_BACK_OFF:
	case STC_VOICE_CLOSER:
	{
		// Along the line from the star kept or pointed at, so it stays where it is in view.
		const bool bBack = Command.action == STC_VOICE_BACK_OFF;
		Star = Pointer->GetTarget();
		if (Star == INDEX_NONE)
		{
			if (!bBack)
			{
				return TEXT("Point at a star first");
			}
			MoveHeadTo(Head - Player->PlayerCameraManager->GetCameraRotation().Vector() * AStarField::UnitsPerParsec);
			return TEXT("Back 1 pc");
		}
		const FVector Position = StarField->GetPosition(Star);
		const FVector Away = Head - Position;
		const double Parsecs = FMath::Max(Away.Size() / AStarField::UnitsPerParsec * (bBack ? DistanceFactor : 1.0 / DistanceFactor), NearestParsecs);
		const FVector Target = Position + Away.GetSafeNormal() * Parsecs * AStarField::UnitsPerParsec;
		MoveHeadTo(Target);
		Face(Star, Target);
		// Two decimals near a star, where "closer" goes as far as 0.02 pc; fewer further out.
		const FString Distance = Parsecs < 1.0	  ? FString::Printf(TEXT("%.2f"), Parsecs)
							   : Parsecs < 10.0 ? FString::Printf(TEXT("%.1f"), Parsecs)
												: FString::Printf(TEXT("%.0f"), Parsecs);
		return FString::Printf(TEXT("%s pc from %s"), *Distance, *StarField->GetName(Star));
	}
	case STC_VOICE_ALIGN_GALACTIC:
	case STC_VOICE_ALIGN_CELESTIAL:
	{
		if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
		{
			return TEXT("In a headset the head decides which way is up");
		}
		const bool bGalactic = Command.action == STC_VOICE_ALIGN_GALACTIC;
		// The galactic north pole, RA 192.85948 deg, Dec +27.12825 deg (ICRS); y flipped
		// into this app's left-handed frame, as for the stars.
		const StcVec3 Pole = bGalactic ? stc_icrs_position(192.85948, 27.12825, 1.0) : StcVec3{0.0, 0.0, 1.0};
		Pointer->SetViewUp(FVector(Pole.x, -Pole.y, Pole.z));
		return bGalactic ? TEXT("Galactic north is up") : TEXT("Celestial north is up");
	}
	case STC_VOICE_LET_GO:
		Pointer->Keep(INDEX_NONE);
		return TEXT("Let go");
	case STC_VOICE_RETURN_TO_SUN:
	{
		// Home, and the Sun in view: SunViewParsecs from it, back along the way the head
		// faces, facing it, and kept. Standing on the Sun hides it (at no distance it is not
		// drawn); this close the rest of the sky is still Earth's, Procyon at 3.5 pc moving
		// less than a degree.
		const FVector Sun = Star != INDEX_NONE ? StarField->GetPosition(Star) : FVector::ZeroVector;
		const FVector Target = Sun - Player->PlayerCameraManager->GetCameraRotation().Vector() * SunViewParsecs * AStarField::UnitsPerParsec;
		MoveHeadTo(Target);
		if (Star != INDEX_NONE)
		{
			Face(Star, Target);
			Pointer->Keep(Star);
		}
		return TEXT("Back at the Sun");
	}
	case STC_VOICE_SHOW:
	case STC_VOICE_GO_TO:
		if (Star == INDEX_NONE)
		{
			const FString Target = UTF8_TO_TCHAR(Command.target);
			return Target.IsEmpty() ? FString::Printf(TEXT("\"%s\": which star?"), *Heard) : FString::Printf(TEXT("No star called \"%s\""), *Target);
		}
		Face(Star, Command.action == STC_VOICE_GO_TO ? GoNear(Star) : Head);
		Pointer->Keep(Star);
		return FString::Printf(TEXT("%s %s"), Command.action == STC_VOICE_GO_TO ? TEXT("Going to") : TEXT("Showing"), *Name);
	default:
		return FString::Printf(TEXT("\"%s\": not a command"), *Heard);
	}
}

FVector AStarActions::GoNear(int32 Star)
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	const FVector Head = Player->PlayerCameraManager->GetCameraLocation();
	const FVector Position = StarField->GetPosition(Star);
	const FVector Target = Position - (Position - Head).GetSafeNormal() * ArriveParsecs * AStarField::UnitsPerParsec;
	MoveHeadTo(Target);
	return Target;
}

void AStarActions::Face(int32 Star, const FVector& From)
{
	// On the desktop only: in a headset the wearer's head decides where they look.
	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		return;
	}
	GetWorld()->GetFirstPlayerController()->SetControlRotation(Pointer->ViewRotation(StarField->GetPosition(Star) - From));
}

void AStarActions::MoveHeadTo(const FVector& Target)
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = Player != nullptr ? Player->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return;
	}
	// In a headset the head is the pawn plus the tracked offset; move the pawn by the
	// difference so the head, not the floor under it, arrives.
	const FVector Head = Player->PlayerCameraManager->GetCameraLocation();
	Pawn->SetActorLocation(Pawn->GetActorLocation() + (Target - Head));
	if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
	{
		Movement->StopMovementImmediately();
	}
}
