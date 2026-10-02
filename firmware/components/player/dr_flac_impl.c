/* Compiles the dr_flac implementation once, kept apart so its warnings
 * settings don't leak into our own code. */
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_OGG
/* Seek past embedded album art instead of reading it into RAM: a cover can
 * be hundreds of KB, and the Cardputer has no PSRAM. */
#define DR_FLAC_NO_PICTURE_METADATA_MALLOC
#include "third_party/dr_flac.h"
