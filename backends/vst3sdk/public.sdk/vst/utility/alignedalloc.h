//------------------------------------------------------------------------
#pragma once
#include "pluginterfaces/base/ftypes.h"
#include <cstdlib>

#if __APPLE__
#include <AvailabilityMacros.h>
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#endif

namespace Steinberg {
namespace Vst {

inline void* aligned_alloc (size_t numBytes, uint32_t alignment)
{
    if (alignment == 0)
        return std::malloc (numBytes);

    void* data {nullptr};

#if SMTG_OS_MACOS && defined(MAC_OS_X_VERSION_MIN_REQUIRED) && MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_15
    posix_memalign (&data, alignment, numBytes);
#elif defined(_MSC_VER) || defined(__MINGW32__)
    // Solução para GCC/MinGW 15
    data = _aligned_malloc (numBytes, alignment);
#else
    data = std::aligned_alloc (alignment, numBytes);
#endif

    return data;
}

inline void aligned_free (void* addr, uint32_t alignment)
{
    if (alignment == 0)
    {
        std::free (addr);
        return;
    }

#if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free (addr);
#else
    std::free (addr);
#endif
}

} // namespace Vst
} // namespace Steinberg