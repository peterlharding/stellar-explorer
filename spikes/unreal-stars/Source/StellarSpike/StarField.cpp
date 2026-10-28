#include "StarField.h"

#include "Materials/MaterialInterface.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ProceduralMeshComponent.h"
#include "SQLiteDatabase.h"

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
	Mesh->SetMaterial(0, Material);
}

bool AStarField::LoadStars(const FString& Path)
{
	FSQLiteDatabase Database;
	if (!Database.Open(*Path, ESQLiteDatabaseOpenMode::ReadOnly))
	{
		UE_LOG(LogStarField, Error, TEXT("Cannot open %s: %s"), *Path, *Database.GetLastError());
		return false;
	}
	// Unreal's wrapper asserts if the database is destroyed while still open.
	ON_SCOPE_EXIT { Database.Close(); };
	// The star view's GROUP BY needs a temporary table, which SQLite would put in a
	// file; an app on visionOS has nowhere it may write one by default.
	Database.Execute(TEXT("PRAGMA temp_store = MEMORY"));

	// Stars with a position and an absolute magnitude; the Sun is at the origin.
	FSQLitePreparedStatement Statement = Database.PrepareStatement(
		TEXT("SELECT x, y, z, absmag, coalesce(teff, 5800) FROM star WHERE x IS NOT NULL AND absmag IS NOT NULL"));
	if (!Statement.IsValid())
	{
		UE_LOG(LogStarField, Error, TEXT("Query failed: %s"), *Database.GetLastError());
		return false;
	}
	ON_SCOPE_EXIT { Statement.Destroy(); };

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> Corners;
	TArray<FVector2D> Magnitudes;
	TArray<FLinearColor> Colours;
	const FVector2D CornerOffsets[4] = {{-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}};

	const int64 Rows = Statement.Execute([&](const FSQLitePreparedStatement& Row)
	{
		double X = 0.0, Y = 0.0, Z = 0.0, AbsMag = 0.0, Teff = 0.0;
		Row.GetColumnValueByIndex(0, X);
		Row.GetColumnValueByIndex(1, Y);
		Row.GetColumnValueByIndex(2, Z);
		Row.GetColumnValueByIndex(3, AbsMag);
		Row.GetColumnValueByIndex(4, Teff);

		// The catalogue frame is right-handed (ICRS); Unreal's is left-handed, so y flips.
		const FVector Position(X * UnitsPerParsec, -Y * UnitsPerParsec, Z * UnitsPerParsec);
		const FLinearColor Colour = BlackbodyColour(Teff);
		const int32 First = Vertices.Num();
		for (const FVector2D& Corner : CornerOffsets)
		{
			Vertices.Add(Position);
			Corners.Add(Corner);
			Magnitudes.Add(FVector2D(AbsMag, 0.0));
			Colours.Add(Colour);
		}
		Triangles.Append({First, First + 1, First + 2, First, First + 2, First + 3});
		return ESQLitePreparedStatementExecuteRowResult::Continue;
	});
	if (Rows == INDEX_NONE)
	{
		UE_LOG(LogStarField, Error, TEXT("Reading stars failed: %s"), *Database.GetLastError());
		return false;
	}

	StarCount = Vertices.Num() / 4;
	const TArray<FVector> NoNormals;
	const TArray<FVector2D> NoUVs;
	const TArray<FProcMeshTangent> NoTangents;
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, NoNormals, Corners, Magnitudes, NoUVs, NoUVs, Colours, NoTangents, false);
	return true;
}
