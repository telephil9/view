#define NULL nil

typedef vlong size_t;

typedef s16int int16_t;
typedef u16int uint16_t;
typedef s32int int32_t;
typedef u32int uint32_t;

#define INT32_MAX  0x7fffffff
#define INT_MAX INT32_MAX
#define UINT32_MAX 0xffffffffU
#define UINT_MAX UINT32_MAX

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_MALLOC(x,u) malloc(x)
#define STBIR_FREE(x,u) free(x)
#define STBIR_ASSERT(x) assert(x)
#include "stb_image_resize.h"
