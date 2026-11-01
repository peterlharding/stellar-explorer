// Shared by both star material nodes. Inputs: ToStar (camera to star, Unreal
// units), AbsMag, UnitsPerParsec, LimitingMag, BaseSize (angular radius in
// radians of a star at the limiting magnitude).
//
// Apparent magnitude from absolute magnitude and distance; flux 1 at the
// limiting magnitude. The sprite's area grows as sqrt(flux) and its surface
// brightness as sqrt(flux), so its total light follows the flux.
float Distance = max(length(ToStar), 1e-3);
float DistancePc = max(Distance / UnitsPerParsec, 1e-6);
float Apparent = AbsMag + 5.0 * log10(DistancePc) - 5.0;
float Flux = pow(10.0, -0.4 * (Apparent - LimitingMag));
float Scale = clamp(pow(Flux, 0.25), 0.6, 12.0);
