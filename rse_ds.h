#pragma once
#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <assert.h>

#ifdef __linux__
    #include <sys/time.h>
#endif


namespace rse {

#ifdef _WIN32
    struct TickTock {
        LARGE_INTEGER start_time;
        double freq;
    };

    TickTock Tick() {
        TickTock tick_tock;
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        tick_tock.freq = (double)li.QuadPart;
        QueryPerformanceCounter(&tick_tock.start_time);
        return tick_tock;
    }

    double Tock(const TickTock& tick_tock) {
        LARGE_INTEGER cur_time;
        QueryPerformanceCounter(&cur_time);
        return (double)(cur_time.QuadPart - tick_tock.start_time.QuadPart) / tick_tock.freq;
    }
#elif __linux__
    struct TickTock {
        struct timeval wclk_t;
    };

    TickTock Tick() {
        TickTock tick_tock;
        gettimeofday(&tick_tock.wclk_t, 0);
        return tick_tock;
    }

    double Tock(const TickTock& tick_tock) {
        struct timeval tv;
        gettimeofday(&tv, 0);
        double t = (double)(tv.tv_sec - tick_tock.wclk_t.tv_sec);
        t += (double)(tv.tv_usec - tick_tock.wclk_t.tv_usec) / 1000000;
        return t;
   }
#endif

    struct Bitmap {

        uint8_t* bitmap = nullptr;
        size_t size = 0;
        size_t capacity = 0;

        Bitmap(size_t size_in) {
            Allocate(size_in);
        }

        size_t Size() { return size; }
        size_t Capacity() { return capacity; }
        size_t SizeOf() { return Capacity(); }

        void Allocate(size_t size_in) {
            delete[] bitmap;
            bitmap = nullptr;
            size = size_in;
            capacity = (size / 8) + 1;
            bitmap = new uint8_t[capacity];
            memset(bitmap, 0, sizeof(uint8_t) * capacity);
        }
        uint8_t* Data() { return bitmap; }

        void Set(size_t index) {
            size_t byte_index = index / 8;
            size_t bit_index = index % 8;
            if (byte_index >= capacity) throw;
            bitmap[byte_index] |= 1 << bit_index;
        }

        void Unset(size_t index) {
            size_t byte_index = index / 8;
            size_t bit_index = index % 8;
            if (byte_index >= capacity) throw;
            bitmap[byte_index] &= ~(1 << bit_index);
        }

        bool Get(size_t index) {
            size_t byte_index = index / 8;
            size_t bit_index = index % 8;
            if (byte_index >= capacity) throw;
            return bitmap[byte_index] & (1 << bit_index);
        }

        bool operator[](size_t index) {
            return Get(index);
        }

        void Print() {

            for (size_t i = 0; i < size; i++) {
                size_t byte_index = i / 8;
                size_t bit_index = i % 8;
                int a = bitmap[byte_index] & (1 << bit_index);
               // if (a) printf("1");
                //else printf("0");
            }
           // printf("\n");
        }

        static void Test() {
            rse::Bitmap bitmap(10);
            bitmap.Set(1);
            bitmap.Set(9);
            bitmap.Set(3);
            bitmap.Set(4);
            bitmap.Unset(3);
            bitmap.Print();
            for (int i = 0; i < 10; ++i) if (bitmap[i]) printf("[%d]\n", i);
        }

        ~Bitmap() {
            delete[] bitmap;
        }
    };
}