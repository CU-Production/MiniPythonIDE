include(FetchContent)

set(SDL_STATIC ON)
set(SDL_SHARED OFF)
FetchContent_Declare(SDL3
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL"
        GIT_TAG "release-3.2.18"
)
FetchContent_MakeAvailable(SDL3)

set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "")
FetchContent_Declare(freetype
        GIT_REPOSITORY "https://github.com/freetype/freetype"
        GIT_TAG "VER-2-13-3"
)
FetchContent_MakeAvailable(freetype)
