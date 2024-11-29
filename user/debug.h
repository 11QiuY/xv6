#include <assert.h>
#include <stdarg.h>
#define DEBUG
#ifdef DEBUG

#define LOG(fmt, ...)                                                          \
  { printf("[%s] " fmt "\n", __func__, ##__VA_ARGS__); }

#define ASSERT(cond, fmt, ...)                                                 \
  {                                                                            \
    if (!(cond)) {                                                             \
      printf("[%s] " fmt, __func__, ##__VA_ARGS__);                            \
      assert(0);                                                               \
    }                                                                          \
  }
#endif

#ifndef DEBUG
#define LOG(fmt, ...)                                                          \
  { 1 == 1; }

#define ASSERT(cond, fmt, ...)                                                 \
  { 1 == 1; }

#endif