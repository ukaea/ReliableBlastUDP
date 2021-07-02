#pragma once

const char* PORT_STR = "27050";
const int PORT_NUM = 27050;
constexpr size_t PAYLOAD_SIZE = 1 * 1024 * 1024 * 1024;

namespace rse {

	namespace test {

        // putting tests in one place for now

        bool TestBitmap() {
            rse::Bitmap bitmap(10);
            
            bitmap.Set(1);
            if (bitmap.Get(1) == false) return false;
            bitmap.Set(9);
            if (bitmap.Get(9) == false) return false;
            bitmap.Unset(3);
            if (bitmap.Get(3) == true) return false;

            // need to test the throw at some point

            return true;
        }


		bool TestMemMap() {

			const size_t SIZE = 64;
			static char buffer[SIZE];

			rse::io::MemMap mem_map;
			if (!rse::io::MapMemory("mem_map_test.txt", 64, rse::io::MemMapIO::READ_WRITE, mem_map)) {
				printf("Failed to mem map file\n");
				return false;
			}

			memset(buffer, 'a', 64);
			memcpy(mem_map.ptr, buffer, 64);

			UnmapMemory(mem_map);

			return true;
		}


		const char* PORT_STR = "27050";
		const int PORT_NUM = 27050;
		constexpr size_t PAYLOAD_SIZE = 1 * 1024 * 1024 * 1024;

#ifdef _WIN32
        unsigned __stdcall ThreadReceiver(void* payload) {

            rse::rbudp::WaitToReceive("127.0.0.1", PORT_STR, PORT_NUM);

            return 0;
        }

        unsigned __stdcall ThreadSender(void* payload) {

            rse::TickTock timer = rse::Tick();

            bool result = rse::rbudp::SendFile("send_test.txt", "test.txt", "127.0.0.1", PORT_STR, PORT_NUM, 4096);
            if (!result) return nullptr;
            
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


        void* ThreadReceiver(void* payload) {
            rse::rbudp::WaitToReceive("127.0.0.1", PORT_STR, PORT_NUM);
            return nullptr;
        }

        void* ThreadSender(void* payload) {
            rse::TickTock timer = rse::Tick();

            bool result = rse::rbudp::SendFile("send_test.txt", "test.txt", "127.0.0.1", PORT_STR, PORT_NUM, 4096);
            if (!result) return nullptr;

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

		bool TestRBUDP() {

            printf("Starting Blast UDP...\n");

            if (rse::sk::Startup() == rse::sk::SK_ERROR_SOCKET) {
                return false;
            }

            // Lets write a 2 gig file 
            FILE* file = fopen("send_test.txt", "wb");
            if (file == NULL) {
                rse::sk::Cleanup();
                return false;
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
                return false;
            }
            HANDLE thread_handle_sender = (HANDLE)_beginthreadex(nullptr, 0, &ThreadSender, nullptr, 0, &thread_ID_sender);
            if (thread_handle_sender == 0) {
                printf("Failed to create thread\n");
                CloseHandle(thread_handle_receiver);
                return false;
            }

            WaitForSingleObject(thread_handle_receiver, INFINITE);
            debug_printf("Receiver thread finished\n");
            WaitForSingleObject(thread_handle_sender, INFINITE);
            debug_printf("Sender thread finished\n");

            CloseHandle(thread_handle_receiver);
            CloseHandle(thread_handle_sender);
#elif __linux__

            pthread_t thread_ID_sender;
            pthread_t thread_ID_receiver;
            int return_sender;
            int return_receiver;

            return_sender = pthread_create(&thread_ID_sender, nullptr, ThreadSender, nullptr);
            if (return_sender) {
                printf("Failed to create thread\n");
                return false;
            }
            return_receiver = pthread_create(&thread_ID_receiver, nullptr, ThreadReceiver, nullptr);
            if (return_receiver) {
                printf("Failed to create thread\n");
                return false;
            }
  
            pthread_join(thread_ID_sender, NULL);
            debug_printf("Sender thread finished\n");
            pthread_join(thread_ID_receiver, NULL);
            debug_printf("Receiver thread finished\n");
#endif
            rse::sk::Cleanup();

            // Open the file and check that it contains all 'a'
            size_t test_txt_size = 0;
            char* buffer = rse::io::AllocateIntoBuffer("test.txt", test_txt_size);
            if (buffer == nullptr) {
                printf("failed to open file!\n");
                return false;
            }

            if (test_txt_size < PAYLOAD_SIZE) {
                printf("Fail on reading test.txt due to the size [%lu] [%lu]\n", test_txt_size, PAYLOAD_SIZE);
                free(buffer);
                return false;
            }
            for (size_t i = 0; i < PAYLOAD_SIZE; i++) {
                debug_printf("%c", buffer[i]);
                if (buffer[i] != 'b') {
                    printf("\nFail on reading test.txt [%c][%lu]\n", buffer[i], i);
                    free(buffer);
                    return false;
                }
            }

            free(buffer);
            printf("\nSuccess!\n");
            return true;
		}

	}

}