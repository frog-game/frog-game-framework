#include <assert.h>

#ifndef defmax
#    define defmax(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef defmin
#    define defmin(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define SAFE_DELETE(p)     \
    {                      \
        if (p) {           \
            delete (p);    \
            (p) = nullptr; \
        }                  \
    }
#define SAFE_DELETE_ARRAY(p) \
    {                        \
        if (p) {             \
            delete[](p);     \
            (p) = nullptr;   \
        }                    \
    }
#define SAFE_RELEASE(p)     \
    {                       \
        if (p) {            \
            (p)->Release(); \
            (p) = nullptr;  \
        }                   \
    }

#define CheckPointer(p)       \
    if (p == nullptr) {       \
        assert(p != nullptr); \
        return;               \
    }

#define CheckPointer_Return(p, r) \
    if (p == nullptr) {           \
        assert(p != nullptr);     \
        return r;                 \
    }

#define CheckAssert(p) \
    if (!(p)) {        \
        assert((p));   \
        return;        \
    }

#define CheckAssert_Return(p, r) \
    if (!(p)) {                  \
        assert((p));             \
        return r;                \
    }

#define FloatEqual(f1, f2) (std::fabs((f1) - (f2)) < std::numeric_limits<float>::epsilon())