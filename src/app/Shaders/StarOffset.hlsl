// World position offset: spread the four vertices of a star into a sprite
// facing the camera, with a constant angular size for a given flux; beyond ViewRadius, none,
// so nothing is drawn (StarColour fades it out before then).
#include "StarCommon"
return (Corner.x * Right + Corner.y * Up) * BaseSize * Scale * Distance * (Shown > 0.0 ? 1.0 : 0.0);
