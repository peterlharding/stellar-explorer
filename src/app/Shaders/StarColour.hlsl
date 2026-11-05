// Emissive colour: a soft disc, as bright at its centre as the flux left over after the size.
#include "StarCommon"
float R2 = dot(Corner, Corner);
float Profile = exp(-4.0 * R2) * saturate(1.0 - R2);
// The colour pulled away from grey by Saturation (1 leaves the blackbody's own).
float Grey = dot(Colour, float3(0.2126, 0.7152, 0.0722));
float3 Hue = max(lerp(Grey.xxx, Colour, Saturation), 0.0);
float3 Centre = Hue * (Flux / (Scale * Scale)) * Exposure;
// A star's colour holds however bright it is: above Knee the centre's brightest channel
// eases towards CoreLimit rather than passing it, all three scaled together; below Knee
// nothing changes. Uncapped, the core of a bright star near at hand (Betelgeuse from a
// parsec) overflowed every channel and the tone mapper showed it white; the eye, adapting,
// still sees a red giant red. Only the centre is eased, then shaded out by Profile, so the
// star stays brightest in the middle and fades to its edge rather than a flat disc. Its
// brilliance shows in its size (StarCommon). Below 1, where the tone mapper whitens least.
const float Knee = 0.5;
const float CoreLimit = 0.85;
float Peak = max(Centre.r, max(Centre.g, Centre.b));
float Eased = Peak <= Knee ? Peak : Knee + (CoreLimit - Knee) * (1.0 - exp(-(Peak - Knee) / (CoreLimit - Knee)));
return Centre * (Eased / max(Peak, 1e-6)) * Profile * Shown;
