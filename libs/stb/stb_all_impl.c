// All header-only library implementations for AOT compilation

// STB Image
#define STBI_NO_THREAD_LOCALS 1
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// STB TrueType
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// cgltf
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
