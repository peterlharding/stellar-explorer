// Emissive colour: a soft disc, as bright as the flux left over after the size.
#include "StarCommon"
float R2 = dot(Corner, Corner);
float Profile = exp(-4.0 * R2) * saturate(1.0 - R2);
return Colour * Profile * (Flux / (Scale * Scale)) * Exposure;
