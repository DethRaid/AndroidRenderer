#pragma once

#if _WIN32
#define SAH_BREAKPOINT __debugbreak()
#elif __ANDROID__
#define SAH_BREAKPOINT raise(SIGINT)
#endif
