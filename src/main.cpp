#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <process.h>    /* _beginthread, _endthread */
#elif __linux__
#include <pthread.h>
#endif

#include <cstdint>
#include <new>
#include <vector>

#include "rse_debug.h"
#include "rse_sockets.h"
#include "rse_perf.h"
#include "rse_rbudp.h"
#include "rse_io.h"
#include "rse_ds.h"
#include "rse_tests.h"

// Blast udp
int main() {

    if (!rse::test::TestBitmap()) {
        printf("bitmap test failed\n");
        return false;
    }
    if (!rse::test::TestMemMap()) {
        printf("memmap test failed\n");
        return false;
    }
    if (!rse::test::TestRBUDP()) printf("rbudp test failed\n");

    return 1;
}