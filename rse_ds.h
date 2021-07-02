#pragma once
#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <assert.h>

namespace rse {

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

        void Set(int index) {
            int byte_index = index / 8;
            int bit_index = index % 8;
            if (byte_index >= capacity) throw;
            bitmap[byte_index] |= 1 << bit_index;
        }

        void Unset(int index) {
            int byte_index = index / 8;
            int bit_index = index % 8;
            if (byte_index >= capacity) throw;
            bitmap[byte_index] &= ~(1 << bit_index);
        }

        bool Get(int index) {
            int byte_index = index / 8;
            int bit_index = index % 8;
            if (byte_index >= capacity) throw;
            return bitmap[byte_index] & (1 << bit_index);
        }

        bool operator[](int index) {
            return Get(index);
        }

        void Print() {

            for (int i = 0; i < size; i++) {
                int byte_index = i / 8;
                int bit_index = i % 8;
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