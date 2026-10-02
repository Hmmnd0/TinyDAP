/* Compiles the dr_flac implementation once, kept apart so its warnings
 * settings don't leak into our own code. */
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_OGG
#include "third_party/dr_flac.h"
