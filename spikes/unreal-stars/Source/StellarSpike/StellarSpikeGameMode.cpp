#include "StellarSpikeGameMode.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "HighResScreenshot.h"
#include "IXRTrackingSystem.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "ProceduralMeshComponent.h"
#include "StarField.h"
#include "StarPointer.h"
#include "StarViewerPawn.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogStellarSpike, Log, All);

namespace
{
	// Start 2 pc from the Sun, looking back at it.
	const FVector StartPosition(-2.0 * AStarField::UnitsPerParsec, 0.0, 0.3 * AStarField::UnitsPerParsec);
	// In a headset, 1 pc is 1 m: stand 1.5 m from the Sun with it at about eye height.
	// The tracking origin is the floor, so the pawn sits 1.6 m below the Sun.
	const FVector HeadsetPosition(-150.0, 0.0, -160.0);
	constexpr double AutopilotRadiusParsecs = 6.0;
	constexpr double AutopilotDegreesPerSecond = 12.0;
}

AStellarSpikeGameMode::AStellarSpikeGameMode()
{
	DefaultPawnClass = AStarViewerPawn::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void AStellarSpikeGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// Read here, not in StartPlay: the player's pawn is spawned while the map loads, before StartPlay.
	bAutopilot = FParse::Param(FCommandLine::Get(), TEXT("Autopilot"));
	FParse::Value(FCommandLine::Get(), TEXT("QuitAfter="), QuitAfter);
	FString Target;
	if (FParse::Value(FCommandLine::Get(), TEXT("LookAt="), Target, false))
	{
		FString RaText, DecText;
		if (Target.Split(TEXT(","), &RaText, &DecText))
		{
			const double Ra = FMath::DegreesToRadians(FCString::Atod(*RaText) * 15.0);
			const double Dec = FMath::DegreesToRadians(FCString::Atod(*DecText));
			// ICRS direction, with y flipped into Unreal's left-handed frame as in AStarField.
			LookAt = FVector(FMath::Cos(Dec) * FMath::Cos(Ra), -FMath::Cos(Dec) * FMath::Sin(Ra), FMath::Sin(Dec));
		}
	}
	Super::InitGame(MapName, Options, ErrorMessage);
}

void AStellarSpikeGameMode::StartPlay()
{
	StarField = GetWorld()->SpawnActor<AStarField>();
	GetWorld()->SpawnActor<AStarPointer>()->SetStarField(StarField);
	if (FParse::Param(FCommandLine::Get(), TEXT("TestObject")))
	{
		SpawnTestObject();
	}
	const bool bHeadset = GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();
	if (bHeadset || FParse::Param(FCommandLine::Get(), TEXT("SkySphere")) || FCString::Strifind(FCommandLine::Get(), TEXT("SkySphere=")))
	{
		SpawnSkySphere();
	}
	Super::StartPlay();
}

void AStellarSpikeGameMode::SpawnTestObject()
{
	// 1 m ahead of a headset's starting point and 1.4 m above the floor, about eye height.
	const FVector Position = HeadsetPosition + FVector(100.0, 0.0, 140.0);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube == nullptr)
	{
		UE_LOG(LogStellarSpike, Error, TEXT("Test object: /Engine/BasicShapes/Cube is not in the package"));
		return;
	}
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Position, FRotator(20.0, 30.0, 0.0));
	Actor->SetMobility(EComponentMobility::Movable);
	Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
	// The basic cube is 1 m across; make it 20 cm.
	Actor->SetActorScale3D(FVector(0.2));
	APointLight* Light = GetWorld()->SpawnActor<APointLight>(Position + FVector(-50.0, -30.0, 40.0), FRotator::ZeroRotator);
	Light->SetMobility(EComponentMobility::Movable);
	Light->PointLightComponent->SetIntensity(5000.0f);
	Light->PointLightComponent->SetAttenuationRadius(500.0f);
	UE_LOG(LogStellarSpike, Display, TEXT("Test object at %s"), *Position.ToString());
}

void AStellarSpikeGameMode::SpawnSkySphere()
{
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Material == nullptr)
	{
		UE_LOG(LogStellarSpike, Error, TEXT("Sky sphere: /Engine/BasicShapes/BasicShapeMaterial is not in the package"));
		return;
	}
	// By default beyond the furthest star, about 1,000 pc. Nothing lights it, so it is black.
	double RadiusParsecs = 1200.0;
	FParse::Value(FCommandLine::Get(), TEXT("SkySphere="), RadiusParsecs);
	const double Radius = RadiusParsecs * AStarField::UnitsPerParsec;

	// A latitude-longitude sphere. Each triangle is added with both windings, so it is seen
	// from inside whichever way the renderer culls; a negative scale does not do this,
	// because Unreal flips its culling to compensate.
	constexpr int32 Rings = 32, Segments = 64;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	for (int32 Ring = 0; Ring <= Rings; ++Ring)
	{
		const double Polar = PI * Ring / Rings;
		for (int32 Segment = 0; Segment <= Segments; ++Segment)
		{
			const double Azimuth = 2.0 * PI * Segment / Segments;
			Vertices.Add(Radius * FVector(FMath::Sin(Polar) * FMath::Cos(Azimuth), FMath::Sin(Polar) * FMath::Sin(Azimuth), FMath::Cos(Polar)));
		}
	}
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			const int32 A = Ring * (Segments + 1) + Segment, B = A + Segments + 1;
			Triangles.Append({A, B, A + 1, A + 1, B, B + 1});
			Triangles.Append({A, A + 1, B, A + 1, B + 1, B});
		}
	}
	AActor* Sky = GetWorld()->SpawnActor<AActor>();
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Sky, TEXT("SkySphere"));
	Sky->SetRootComponent(Mesh);
	Mesh->RegisterComponent();
	Mesh->SetCastShadow(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const TArray<FVector> NoNormals;
	const TArray<FVector2D> NoUVs;
	const TArray<FColor> NoColours;
	const TArray<FProcMeshTangent> NoTangents;
	Mesh->CreateMeshSection(0, Vertices, Triangles, NoNormals, NoUVs, NoColours, NoTangents, false);
	Mesh->SetMaterial(0, Material);
	UE_LOG(LogStellarSpike, Display, TEXT("Sky sphere of radius %.0f pc"), RadiusParsecs);
}

void AStellarSpikeGameMode::LogCamera() const
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (Player && Player->PlayerCameraManager)
	{
		const FVector Location = Player->PlayerCameraManager->GetCameraLocation();
		const FVector Forward = Player->PlayerCameraManager->GetCameraRotation().Vector();
		UE_LOG(LogStellarSpike, Display, TEXT("Camera at %s (pc), facing %s, FOV %.1f"),
			*(Location / AStarField::UnitsPerParsec).ToString(), *Forward.ToString(), Player->PlayerCameraManager->GetFOVAngle());
	}
}

APawn* AStellarSpikeGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	const bool bHeadset = GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();
	// Just off the Sun when looking at the sky from it, so the Sun itself is behind the camera.
	FVector Position = LookAt ? LookAt.GetValue() * 0.001 : StartPosition;
	FRotator Facing = LookAt ? LookAt->Rotation() : (-StartPosition).Rotation();
	if (bHeadset)
	{
		GEngine->XRSystem->SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
		Position = HeadsetPosition;
		Facing = FRotator::ZeroRotator;
		UE_LOG(LogStellarSpike, Display, TEXT("Headset: %s"), *GEngine->XRSystem->GetSystemName().ToString());
	}
	APawn* Pawn = SpawnDefaultPawnAtTransform(NewPlayer, FTransform(Facing, Position));
	if (AStarViewerPawn* Flyer = Cast<AStarViewerPawn>(Pawn))
	{
		if (bHeadset)
		{
			Flyer->UseHeadset();
		}
		// About 2 pc/s at full speed; the default is tuned for metres, not parsecs.
		if (UFloatingPawnMovement* Movement = Cast<UFloatingPawnMovement>(Flyer->GetMovementComponent()))
		{
			Movement->MaxSpeed = 2.0 * AStarField::UnitsPerParsec;
			Movement->Acceleration = 4.0 * AStarField::UnitsPerParsec;
			Movement->Deceleration = 8.0 * AStarField::UnitsPerParsec;
		}
	}
	if (APlayerController* Player = Cast<APlayerController>(NewPlayer))
	{
		Player->SetControlRotation(Facing);
	}
	return Pawn;
}

void AStellarSpikeGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	// Ignore the first seconds, while shaders compile and the catalogue loads.
	if (Elapsed > 3.0)
	{
		FrameTimes.Add(DeltaSeconds);
	}

	// In a headset, where the view cannot be seen from the Mac, log where it is.
	if (GEngine->XRSystem.IsValid() && Elapsed >= NextCameraLog)
	{
		LogCamera();
		NextCameraLog = Elapsed + 2.0;
	}

	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	// A scripted view must not be steered by wherever the mouse happens to be, so it is set every frame.
	if (LookAt && QuitAfter > 0.0 && Player)
	{
		Player->SetControlRotation(LookAt->Rotation());
	}
	if (bAutopilot && Player && Player->GetPawn())
	{
		const double Angle = FMath::DegreesToRadians(Elapsed * AutopilotDegreesPerSecond);
		const double Radius = AutopilotRadiusParsecs * AStarField::UnitsPerParsec;
		const FVector Position(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.2 * Radius);
		const FRotator Facing = (-Position).Rotation();
		Player->GetPawn()->SetActorLocation(Position);
		Player->SetControlRotation(Facing);
	}

	if (QuitAfter > 0.0 && Elapsed > QuitAfter)
	{
		if (!bScreenshotRequested)
		{
			LogCamera();
			// One file per kind of run, so a later run cannot overwrite another's picture.
			const TCHAR* Name = bAutopilot ? TEXT("StellarSpike-orbit") : LookAt ? TEXT("StellarSpike-look") : TEXT("StellarSpike-view");
			FScreenshotRequest::RequestScreenshot(Name, false, false);
			bScreenshotRequested = true;
		}
		else if (Elapsed > QuitAfter + 1.0)
		{
			Finish();
		}
	}
}

void AStellarSpikeGameMode::Finish()
{
	if (FrameTimes.Num() > 0)
	{
		TArray<float> Sorted = FrameTimes;
		Sorted.Sort();
		const float Median = Sorted[Sorted.Num() / 2];
		const float Worst1Percent = Sorted[FMath::Min(Sorted.Num() - 1, int32(Sorted.Num() * 0.99))];
		UE_LOG(LogStellarSpike, Display, TEXT("Frames: %d, stars: %d, median %.2f ms (%.0f fps), 99th percentile %.2f ms, worst %.2f ms"),
			Sorted.Num(), StarField ? StarField->GetStarCount() : 0, Median * 1000.0f, 1.0f / Median, Worst1Percent * 1000.0f, Sorted.Last() * 1000.0f);
	}
	FPlatformMisc::RequestExit(false, TEXT("QuitAfter"));
}
