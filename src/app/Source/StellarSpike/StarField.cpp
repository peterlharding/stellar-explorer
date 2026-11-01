#include "StarField.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "ProceduralMeshComponent.h"
#include "StarCatalogue.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarField, Log, All);

namespace
{
	/** sRGB colour of a blackbody, after Tanner Helland's fit; good to a few percent from 1000 K to 40000 K. */
	FLinearColor BlackbodyColour(double Kelvin)
	{
		const double T = FMath::Clamp(Kelvin, 1000.0, 40000.0) / 100.0;
		double R, G, B;
		if (T <= 66.0)
		{
			R = 255.0;
			G = 99.4708025861 * FMath::Loge(T) - 161.1195681661;
			B = T <= 19.0 ? 0.0 : 138.5177312231 * FMath::Loge(T - 10.0) - 305.0447927307;
		}
		else
		{
			R = 329.698727446 * FMath::Pow(T - 60.0, -0.1332047592);
			G = 288.1221695283 * FMath::Pow(T - 60.0, -0.0755148492);
			B = 255.0;
		}
		const FColor SRGB(FMath::Clamp(int32(R), 0, 255), FMath::Clamp(int32(G), 0, 255), FMath::Clamp(int32(B), 0, 255));
		return FLinearColor::FromSRGBColor(SRGB);
	}
}

AStarField::AStarField()
{
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Stars"));
	RootComponent = Mesh;
	Mesh->SetCastShadow(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AStarField::BeginPlay()
{
	Super::BeginPlay();

	// The repository's catalogue when run from the source tree; the copy staged into the
	// package (Content/Data, see spike.sh) on a device.
	FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../data/catalogue.sqlite")));
	if (!FPaths::FileExists(Path))
	{
		const FString Staged = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/catalogue.sqlite"));
		Path = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Staged);
	}
	FParse::Value(FCommandLine::Get(), TEXT("Catalogue="), Path);

	const double Start = FPlatformTime::Seconds();
	if (!LoadStars(Path))
	{
		return;
	}
	UE_LOG(LogStarField, Display, TEXT("Loaded %d stars from %s in %.2f s"), StarCount, *Path, FPlatformTime::Seconds() - Start);

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/M_Star.M_Star"));
	if (Material == nullptr)
	{
		UE_LOG(LogStarField, Error, TEXT("/Game/Generated/M_Star is missing: run Scripts/make_star_material.py"));
		return;
	}
	// The material's parameters can be set from the command line, for tuning on a device
	// without regenerating the material: -StarExposure=, -StarSize= (BaseSize), -LimitingMag=.
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
	// The material's exposure suits a monitor; the Vision Pro, judged by eye, wants about six times more.
	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		Instance->SetScalarParameterValue(TEXT("Exposure"), HeadsetExposure);
	}
	for (const TPair<const TCHAR*, FName>& Option : {TPair<const TCHAR*, FName>(TEXT("StarExposure="), TEXT("Exposure")),
			 TPair<const TCHAR*, FName>(TEXT("StarSize="), TEXT("BaseSize")), TPair<const TCHAR*, FName>(TEXT("LimitingMag="), TEXT("LimitingMag"))})
	{
		float Value = 0.0f;
		if (FParse::Value(FCommandLine::Get(), Option.Key, Value))
		{
			Instance->SetScalarParameterValue(Option.Value, Value);
			UE_LOG(LogStarField, Display, TEXT("%s = %g"), *Option.Value.ToString(), Value);
		}
	}
	Mesh->SetMaterial(0, Instance);
}

bool AStarField::LoadStars(const FString& Path)
{
	// Catalogue access lives in the engine-independent core now (ADR 0014), reached through
	// its C ABI; FStarCatalogue wraps the handle and loads every usable star.
	FStarCatalogue Catalogue(Path);
	if (!Catalogue.IsOpen())
	{
		UE_LOG(LogStarField, Error, TEXT("Cannot open catalogue %s"), *Path);
		return false;
	}
	const TArray<StcStar>& Stars = Catalogue.Stars();

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> Corners;
	TArray<FVector2D> Magnitudes;
	TArray<FLinearColor> Colours;
	const FVector2D CornerOffsets[4] = {{-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}};

	Positions.Reserve(Stars.Num());
	AbsoluteMagnitudes.Reserve(Stars.Num());
	Names.Reserve(Stars.Num());
	SpectralTypes.Reserve(Stars.Num());
	Vertices.Reserve(Stars.Num() * 4);
	Triangles.Reserve(Stars.Num() * 6);
	Corners.Reserve(Stars.Num() * 4);
	Magnitudes.Reserve(Stars.Num() * 4);
	Colours.Reserve(Stars.Num() * 4);

	for (const StcStar& Star : Stars)
	{
		// The catalogue frame is right-handed (ICRS); Unreal's is left-handed, so y flips.
		const FVector Position(Star.x * UnitsPerParsec, -Star.y * UnitsPerParsec, Star.z * UnitsPerParsec);
		const FLinearColor Colour = BlackbodyColour(Star.teff);
		Positions.Add(Position);
		AbsoluteMagnitudes.Add(Star.absmag);
		Names.Add(FString(UTF8_TO_TCHAR(Star.name)));
		SpectralTypes.Add(FString(UTF8_TO_TCHAR(Star.spectral_type)));
		const int32 First = Vertices.Num();
		for (const FVector2D& Corner : CornerOffsets)
		{
			Vertices.Add(Position);
			Corners.Add(Corner);
			Magnitudes.Add(FVector2D(Star.absmag, 0.0));
			Colours.Add(Colour);
		}
		Triangles.Append({First, First + 1, First + 2, First, First + 2, First + 3});
	}

	StarCount = Stars.Num();
	const TArray<FVector> NoNormals;
	const TArray<FVector2D> NoUVs;
	const TArray<FProcMeshTangent> NoTangents;
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, NoNormals, Corners, Magnitudes, NoUVs, NoUVs, Colours, NoTangents, false);
	return true;
}

double AStarField::ApparentMagnitude(double AbsMag, double Distance)
{
	// As in Shaders/StarCommon.hlsl.
	const double DistanceParsecs = FMath::Max(Distance / UnitsPerParsec, 1e-6);
	return AbsMag + 5.0 * FMath::LogX(10.0, DistanceParsecs) - 5.0;
}

int32 AStarField::FindStar(const FVector& Origin, const FVector& Direction, double MaxAngleDegrees) const
{
	const double MinCosine = FMath::Cos(FMath::DegreesToRadians(MaxAngleDegrees));
	int32 Best = INDEX_NONE;
	double BestMagnitude = TNumericLimits<double>::Max();
	for (int32 Star = 0; Star < Positions.Num(); ++Star)
	{
		const FVector ToStar = Positions[Star] - Origin;
		const double Distance = ToStar.Size();
		// Nearer than the near clipping plane, a star is not drawn, so cannot be pointed at.
		if (Distance < GNearClippingPlane || FVector::DotProduct(ToStar, Direction) < MinCosine * Distance)
		{
			continue;
		}
		const double Magnitude = ApparentMagnitude(AbsoluteMagnitudes[Star], Distance);
		if (Magnitude < BestMagnitude)
		{
			BestMagnitude = Magnitude;
			Best = Star;
		}
	}
	return Best;
}
