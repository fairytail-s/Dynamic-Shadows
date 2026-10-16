#pragma once
#if defined(__GNUC__) || defined(__clang__)
#define PL_EXPORT [[maybe_unused]] __attribute__((visibility("default")))
#else
#define PL_EXPORT [[maybe_unused]]
#endif
