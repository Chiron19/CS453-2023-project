/**
 * @file   test.cpp
 * @author []
 *
 * @section LICENSE
 *
 * Copyright © 2018-2019 Sébastien Rouault.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version. Please see https://gnu.org/licenses/gpl.html
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * @section DESCRIPTION
 *
 * Trivial program that call a function in several threads.
**/

// External headers
#include <atomic>
#include <iostream>
#include <thread>
#include <mutex>
#include <fstream>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <tuple>
#include <vector>

// Internal headers
#include "tm.cpp"

int TX_num = 10;
std::vector<int> accounts;
std::vector<std::tuple<int, int, int>> transactions;
shared_t tm;

// Forward declaration
void shared_access(); 
static void shared_check();

// -------------------------------------------------------------------------- //
/* Initialize the array of accounts,
at the "start address" of the shared memory */
void transfer(int src, int dst, int amount) {
    std::atomic_int *src_account = new std::atomic_int();
    std::atomic_int *dst_account = new std::atomic_int();
    while (true) {
        try {
            tx_t tx = tm_begin(tm, true);
            tm_read(tm, tx, &accounts[src], sizeof(int), src_account);
            if (*src_account < amount) {
                tm_end(tm, tx);
                break;
            }
            tm_read(tm, tx, &accounts[src], sizeof(int), src_account);
            tm_read(tm, tx, &accounts[dst], sizeof(int), dst_account);
            tm_write(tm, tx, &accounts[dst], sizeof(int), dst_account + amount);
            tm_write(tm, tx, &accounts[src], sizeof(int), src_account - amount);
            std::cout << "Transfer " << amount << " " << src << "->" << dst << std::endl;
            tm_end(tm, tx);
            break;
        } catch (std::exception e) {
            perror(e.what());
            continue;
        }
    }
    free(src_account);
    free(dst_account);
}

// -------------------------------------------------------------------------- //
// Thread accessing the shared memory (a mere shared counter in this program)

/** Thread entry point.
 * @param nb   Total number of threads
 * @param id   This thread ID (from 0 to nb-1 included)
 * @param lock Lock to use to protect the shared memory (read & written by 'shared_access')
**/
void entry_point(size_t nb, size_t id) {
    ::printf("Hello from thread %lu/%lu\n", id, nb);
    for (size_t i = 0; i < transactions.size(); i++)
    {
        auto [src_id, dst_id, tx_amount] = transactions[i];
        if (src_id == id) transfer(src_id, dst_id, tx_amount);
    }
}

// -------------------------------------------------------------------------- //
// Shared memory, access function and consistency check

static int counter = 0;
static ::std::atomic<int> check_counter{0};

/** Performs some operations on some shared memory.
**/
void shared_access() {
    ++counter;
    check_counter.fetch_add(1, ::std::memory_order_relaxed);
}

/** (Empirically) checks that concurrent operations did not break consistency, warn accordingly.
**/
static void shared_check() {
    auto calls = check_counter.load(::std::memory_order_relaxed);
    if (counter == calls) {
        ::std::cout << "** No inconsistency detected (" << counter << " == " << calls << ") **" << ::std::endl;
    } else {
        ::std::cout << "** Inconsistency detected (" << counter << " != " << calls << ") **" << ::std::endl;
    }
}

void transaction_generation(int n, int tx_num, std::string outputFilePath) {
    srand(time(NULL)); // Initialize random seed

    std::ofstream file(outputFilePath);
    if (!file) {
        std::cerr << "Unable to open file: " << outputFilePath << std::endl;
        return;
    }

    // Write n and tx_num
    file << n << " " << tx_num << "\n";

    // Generate random balance for n peers
    for (int i = 0; i < n; ++i) {
        file << rand() % 1000 << " "; // Generate random balance between 0 and 999
    }
    file << "\n";

    // Generate tx_num transactions
    for (int i = 0; i < tx_num; ++i) {
        int src_id = rand() % n; // Generate random source id
        int dst_id;
        do {
            dst_id = rand() % n; // Generate random destination id
        } while (dst_id == src_id); // Guarantee dst_id != src_id

        int tx_amount = rand() % 1000; // Generate random transaction amount between 0 and 999

        file << src_id << " " << dst_id << " " << tx_amount << "\n";
    }

    file.close();
}

void transaction_init(std::string inputFilePath) {
    std::ifstream file(inputFilePath);
    if (!file) {
        std::cerr << "Unable to open file: " << inputFilePath << std::endl;
        return;
    }

    int n, tx_num;
    file >> n >> tx_num;

    accounts.resize(n);
    for (int i = 0; i < n; ++i) {
        file >> accounts[i];
    }

    transactions.resize(tx_num);
    for (int i = 0; i < tx_num; ++i) {
        int src_id, dst_id, tx_amount;
        file >> src_id >> dst_id >> tx_amount;
        transactions[i] = std::make_tuple(src_id, dst_id, tx_amount);
    }

    file.close();
}

// -------------------------------------------------------------------------- //
// Thread launches and management

/** Program entry point.
 * @param argc Arguments count
 * @param argv Arguments values
 * @return Program return code
**/
int main(int, char**) {
    // Init threads
    auto const nbworkers = []() {
        auto res = ::std::thread::hardware_concurrency();
        if (res == 0) {
            ::std::cout << "WARNING: unable to query '::std::thread::hardware_concurrency()', falling back to 4 threads" << ::std::endl;
            res = 4;
        }
        return static_cast<size_t>(res);
    }();

    // Init transactions
    ::std::thread threads[nbworkers];
    transaction_generation(nbworkers, TX_num, "test.txt");
    transaction_init("test.txt");
    for (size_t i = 0; i < accounts.size(); i++)
    {
        std::cout << accounts[i] << " ";
    }
    putchar(10);

    // Init shared memory
    tm = tm_create(transactions.size() * sizeof(struct word), sizeof(struct word));
    if (tm == invalid_shared) {
        ::std::cerr << "ERROR: unable to create shared memory" << ::std::endl;
        return 1;
    }
    std::cout << "Shared memory size: " << tm_size(tm) << std::endl;
    std::cout << "Shared memory align: " << tm_align(tm) << std::endl;
    std::cout << "Shared memory start: " << tm_start(tm) << std::endl;
    
    // Launch threads
    for (size_t i = 0; i < nbworkers; ++i) {
        threads[i] = ::std::thread{[&](size_t i) {
            entry_point(nbworkers, i);
        }, i};
    }
    
    // Wait for threads to finish
    for (auto&& thread: threads) thread.join();
    shared_check();

    for (size_t i = 0; i < accounts.size(); i++)
    {
        std::cout << accounts[i] << " ";
    }
    putchar(10);

    // Destroy shared memory
    try {
        tm_destroy(tm);
    } catch(const std::exception& e) {}

    return 0;
}