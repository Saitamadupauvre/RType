#if defined(_WIN32)
#define SAMPLE_EXPORT extern "C" __declspec(dllexport)
#else
#define SAMPLE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

SAMPLE_EXPORT int sample_answer() { return 42; }
