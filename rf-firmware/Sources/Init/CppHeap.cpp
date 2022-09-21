#include <stddef.h>
#include <stdlib.h>

void *operator new(size_t numBytes) {
    return malloc(numBytes);
}

void operator delete(void* p) {
    free(p);
}

// Same as above, just a C++14 specialization.
// (See http://en.cppreference.com/w/cpp/memory/new/operator_delete)
void operator delete(void* p, size_t t) {
    free(p);
}
