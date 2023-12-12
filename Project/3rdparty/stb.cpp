#if defined(_MSC_VER)
#	pragma warning(push, 0)
#	pragma warning(disable : 5045)
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>
#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_BOX
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize2.h>

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif