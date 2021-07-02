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

const char* PORT_STR = "27050";
const int PORT_NUM = 27050;
//constexpr size_t PAYLOAD_SIZE = 1 * 1024 * 1024 * 1024;
constexpr size_t PAYLOAD_SIZE = 64;

#ifdef _WIN32
unsigned __stdcall ThreadReceiver(void* payload) {

    rse::rbudp::WaitToReceive("127.0.0.1", PORT_STR, PORT_NUM);

    return 0;
}

unsigned __stdcall ThreadSender(void* payload) {

    rse::TickTock timer = rse::Tick();

    rse::rbudp::SendFile("send_test.txt", "test.txt", "127.0.0.1", PORT_STR, PORT_NUM, 4096);

    double t = rse::Tock(timer);
    fprintf(stdout, "[%lf]\n", t);

    double mbytes = (double)PAYLOAD_SIZE / (double)1024 / (double)1024;
    double mbytes_per_sec = mbytes / t;
    double mbits_per_sec = (mbytes * 8) / t;

    fprintf(stdout, "[%lf] MByteps\n", mbytes_per_sec);
    fprintf(stdout, "[%lf] MBitsps\n", mbits_per_sec);

    return 0;
}
#elif __linux__


void *ThreadReceiver( void *payload ) {
    rse::rbudp::WaitToReceive("127.0.0.1", PORT_STR, PORT_NUM);

    return nullptr;
}

void *ThreadSender( void* payload ) {
    rse::TickTock timer = rse::Tick();

    rse::rbudp::SendFile("send_test.txt", "test.txt", "127.0.0.1", PORT_STR, PORT_NUM, 4096);

    double t = rse::Tock(timer);
    fprintf(stdout, "[%lf]\n", t);

    double mbytes = (double)PAYLOAD_SIZE / (double)1024 / (double)1024;
    double mbytes_per_sec = mbytes / t;
    double mbits_per_sec = (mbytes * 8) / t;

    fprintf(stdout, "[%lf] MByteps\n", mbytes_per_sec);
    fprintf(stdout, "[%lf] MBitsps\n", mbits_per_sec);

    return nullptr;
}

#endif

// Blast udp
int main() {

    printf("Starting Blast UDP...\n");

    if (rse::sk::Startup() == rse::sk::SK_ERROR_SOCKET) {
        return 0;
    }

    // Lets write a 2 gig file 
    FILE* file = fopen("send_test.txt", "wb");
    if (file == NULL) {
        rse::sk::Cleanup();
        return 0;
    }

    char* data = new char[PAYLOAD_SIZE];
    memset(data, 'b', PAYLOAD_SIZE);
    fwrite(data, 1, PAYLOAD_SIZE, file);
    fclose(file);
    delete[] data;

#ifdef _WIN32
    unsigned thread_ID_sender;
    unsigned thread_ID_receiver;

    HANDLE thread_handle_receiver = (HANDLE)_beginthreadex(nullptr, 0, &ThreadReceiver, nullptr, 0, &thread_ID_receiver);
    if (thread_handle_receiver == 0) {
        printf("Failed to create thread\n");
        return 0;
    }
    HANDLE thread_handle_sender = (HANDLE)_beginthreadex(nullptr, 0, &ThreadSender, nullptr, 0, &thread_ID_sender);
    if (thread_handle_sender == 0) {
        printf("Failed to create thread\n");
        CloseHandle(thread_handle_receiver);
        return 0;
    }

    WaitForSingleObject(thread_handle_receiver, INFINITE);
    WaitForSingleObject(thread_handle_sender, INFINITE);

    CloseHandle(thread_handle_receiver);
    CloseHandle(thread_handle_sender);
#elif __linux__

    pthread_t thread_ID_sender;
    pthread_t thread_ID_receiver;
    int return_sender;
    int return_receiver;

    return_sender = pthread_create( &thread_ID_sender, nullptr, ThreadSender, nullptr);
    if (return_sender) {
        printf("Failed to create thread\n");
        return 0;
    }
    return_receiver = pthread_create( &thread_ID_receiver, nullptr, ThreadReceiver, nullptr);
    if (return_receiver) {
        printf("Failed to create thread\n");
        return 0;
    }

    pthread_join( thread_ID_receiver, NULL);
	pthread_join( thread_ID_sender, NULL);

#endif
    rse::sk::Cleanup();

    // Open the file and check that it contains all 'a'
    size_t test_txt_size = 0;
    char* buffer = rse::io::AllocateIntoBuffer("test.txt", test_txt_size);
    if (buffer == nullptr) {
        printf("failed to open file!\n");
        return 0;
    }

    if (test_txt_size < PAYLOAD_SIZE) {
        printf("Fail on reading test.txt due to the size [%lu] [%lu]\n", test_txt_size, PAYLOAD_SIZE);
        free(buffer);
        return 0;
    }
    for (size_t i = 0; i < PAYLOAD_SIZE; i++) {
        debug_printf("%c", buffer[i]);
        if (buffer[i] != 'b') {
            printf("\nFail on reading test.txt [%c][%lu]\n", buffer[i], i);
            free(buffer);
            return 0;
        }
    }

    free(buffer);
    printf("\nSuccess!\n");

    //int bytes_per_sec = (double)(BLOCK_SIZE * NUMBER_PACKETS) / t;
    //fprintf(stdout, "[%d bps]\n", bytes_per_sec);
    //fprintf(stdout, "[%d kbps]\n", bytes_per_sec / 1024);
    //fprintf(stdout, "[%d mbps]\n", bytes_per_sec / 1024 / 1024);


    return 1;
}