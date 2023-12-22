/**
 * @file   tm.c
 * @author [...]
 *
 * @section LICENSE
 *
 * [...]
 *
 * @section DESCRIPTION
 *
 * Implementation of your own transaction manager.
 * You can completely rewrite this file (and create more files) as you wish.
 * Only the interface (i.e. exported symbols and semantic) must be preserved.
**/

// Requested features
#ifndef TM_CPP
#define TM_CPP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// External headers

// Internal headers
#include "tm.hpp"

#include "macros.h"

static const tx_t read_only_tx  = UINTPTR_MAX - 10;
static const tx_t read_write_tx = UINTPTR_MAX - 11;


/**
 * @brief Simple Shared Memory Region (a.k.a Transactional Memory).
 */
struct region {
    void* start;        // Start of the shared memory region (i.e., of the non-deallocable memory segment)
    Alloc allocs;       // Shared memory transactions status
    size_t size;        // Size of the non-deallocable memory segment (in bytes)
    size_t align;       // Size of a word in the shared memory region (in bytes)
};

/**
 * @brief Atomic Word.
 */
struct word {
    std::atomic_bool valid; // 0: copy_A is valid, 1: copy_B is valid (valid means "readable copy")
    std::atomic_bool accessed, written;
    std::atomic_uint32_t copy_A, copy_B;
};

/**
 * @brief List of dynamically allocated segments.
 */
std::list<region*> segment_list;

/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) {
    std::cout << "tm_create" << std::endl;
    struct region* region = (struct region*) malloc(sizeof(struct region));
    if (unlikely(!region)) {
        return invalid_shared;
    }
    // We allocate the shared memory buffer such that its words are correctly
    // aligned.
    // int ret = 0;
    // if (ret = posix_memalign(&(region->start), align, size) != 0) {
    //     free(region);
    //     // posix_memalign failed
    //     if (ret == EINVAL) {
    //         printf("The alignment argument is not a power of two multiple of sizeof(void*)\n");
    //     } else if (ret == ENOMEM) {
    //         printf("Insufficient memory to fulfill the request\n");
    //     } else {
    //         printf("Unknown error\n");
    //     }
    //     return invalid_shared;
    // }
    memset(region->start, 0, size);
    region->allocs      = static_cast<Alloc>(NULL);
    region->size        = size;
    region->align       = align;
    return region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    struct region* shared_region = (struct region*) shared;
    try {
        // if (shared_region->start != NULL) {
        //     free(shared_region->start);
        //     shared_region->start = NULL;
        // }
        free(shared_region);
    } catch (const std::exception& e) {
        perror(e.what());
        throw;
    }
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) {
    // std::cout << "tm_start" << std::endl;
    return ((struct region*) shared)->start;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) {
    // std::cout << "tm_size" << std::endl;
    return ((struct region*) shared)->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) {
    // std::cout << "tm_align" << std::endl;
    return ((struct region*) shared)->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t shared, bool is_ro) {
    std::cout << "tm_begin" << std::endl;
    // srand(time(NULL));
    // uint* ptr = (uint*) malloc(sizeof(uint));
    // *ptr = rand();
    tx_t tx = is_ro ? read_only_tx : read_write_tx;
    if (unlikely(!tx)) {
        return invalid_tx;
    }
    return tx;
} 

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t tx) {
    std::cout << "tm_end" << std::endl;
    return true;
}

struct word* tm_find_word_by_index(shared_t shared, size_t index) {
    std::cout << "tm_find_word_by_index" << std::endl;
    struct region* shared_region = (struct region*) shared;
    // Check if the index is within the region size
    if (index * shared_region->align >= shared_region->size) {
        return NULL; // Index out of bounds
    }

    // Cast the address to a word pointer and return it
    return (struct word*)shared_region->start + index * shared_region->align;
}

bool tm_read_word(shared_t shared, size_t index, size_t size, void* target, bool is_ro) {
    std::cout << "tm_read_word" << std::endl;
    struct word* source = (struct word*) tm_find_word_by_index(shared, index);
    if (source == NULL) {
        return false;
    }
    std::cout << source->valid << " " << source->accessed << " " << source->written << " " << source->copy_A << " " << source->copy_B << std::endl;
    if (is_ro) {
        // read the readable copy into target
        * reinterpret_cast<uint*>(target) = (source->valid.load() ? static_cast<uint>(source->copy_B.load()) : static_cast<uint>(source->copy_A.load()));
        std::cout << * (uint*)target << std::endl;
        return true;
    } else {
        if (source->written) {
            if (source->accessed) {
                // read the writable copy into target
                * reinterpret_cast<uint*>(target) = (source->valid.load() ? static_cast<uint>(source->copy_A.load()) : static_cast<uint>(source->copy_B.load()));
                std::cout << * (uint*)target << std::endl;

                return true;
            } else {
                return false;
            }
        } else {
            // read the readable copy into target
            * reinterpret_cast<uint*>(target) = (source->valid.load() ? static_cast<uint>(source->copy_B.load()) : static_cast<uint>(source->copy_A.load()));
            std::cout << * (uint*)target << std::endl;

            source->accessed.store(true);
            return true;
        }
    }
    return false;
}

bool tm_write_word(shared_t shared, size_t index, void const* source, size_t size, bool is_ro) {
    std::cout << "tm_write_word" << std::endl;
    struct word* target = (struct word*) tm_find_word_by_index(shared, index);
    if (target == NULL) {
        return false;
    }
    if (is_ro) {
        return false;
    } else {
        if (target->written.load()) {
            if (target->accessed.load()) {
                // write the writable copy
                if (target->valid.load()) {
                    target->copy_A.store(*(uint*)source);
                } else {
                    target->copy_B.store(*(uint*)source);
                }
                target->written.store(true);
                return true;
            } else return false;
        } else {
            if (target->accessed.load()) {
                // if at least one other transaction is in the “access set”, abort
                return false;
            } else {
                // write the writable copy
                if (target->valid.load()) {
                    target->copy_A.store(*(uint*)source);
                } else {
                    target->copy_B.store(*(uint*)source);
                }
                target->written.store(true);
                target->accessed.store(true);
                return true;
            }
        }
    }
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t shared, tx_t unused(tx), void const* unused(source), size_t size, void* target) {
    std::cout << "tm_read" << std::endl;
// foreach word index within [source, source + size] do
// result = read word(word index , target + offset);
// if result = transaction must abort then
// return the transaction must abort;
// end
// end
    struct region* shared_region = (struct region*) shared;
    for (size_t index = 0; index * shared_region->align < shared_region->size; index++)
    {
        if (!tm_read_word(shared, index, size, target, true)) {
            shared_region->allocs = Alloc::abort;
            return false;
        }
        target = (uint*)target + shared_region->align;
    }
    return true;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t shared, tx_t unused(tx), void const* source, size_t size, void* target) {
    std::cout << "tm_write" << std::endl;
// foreach word index within [target, target + size] do
// result = write word(source + offset, word index );
// if result = transaction must abort then
// return the transaction must abort;
// end
// end
// return the transaction can continue;
    struct region* shared_region = (struct region*) shared;
    for (size_t index = 0; index * shared_region->align < shared_region->size; index++)
    {
        if (!tm_write_word(shared, index, source, size, false)) {
            shared_region->allocs = Alloc::abort;
            return false;
        }
        target = (uint*)target + shared_region->align;
    }
    return true;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
Alloc tm_alloc(shared_t shared, tx_t unused(tx), size_t size, void** target) {
    std::cout << "tm_alloc" << std::endl;
// allocate enough space for a segment of size size;
// if the allocation failed then
// return the allocated failed;
// end
// initialize the control structure(s) (one per word) in the segment;
// initialize each copy of each word in the segment to zeroes;
// register the segment in the set of allocated segments;
// return the transaction can continue;
    size_t align = tm_align(shared);

    struct region* shared_region = (struct region*) shared;

    // Allocate the segment
    try {
        segment_list.push_back(shared_region);
    } catch (const std::bad_alloc& e) { // Allocation failed
        return Alloc::nomem;
    }

    void* segment = (void*) (shared_region->start);
    memset(segment, 0, size);

    // Initialize each align-sized block as a struct word
    size_t num_words = size / align;
    for (size_t i = 0; i < num_words; i++) {
        void* addr = (char*)segment + i * align;
        struct word* w = (struct word*)addr;
        w->valid.store(false);
        w->accessed.store(false);
        w->written.store(false);
        w->copy_A.store((size_t)malloc(align));
        w->copy_B.store((size_t)malloc(align));
    }

    *target = segment;
    return Alloc::success;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t shared, tx_t unused(tx), void* unused(target)) {
    std::cout << "tm_free" << std::endl;
// mark target for deregistering (from the set of allocated segments)
// and freeing once the last transaction of the current epoch leaves
// the Batcher, if the calling transaction ends up being committed;
// return the transaction can continue;
    struct region* shared_region = (struct region*) shared;
    std::list<region*>::iterator it = std::find(segment_list.begin(), segment_list.end(), shared_region);
    try
    {
        if (it != segment_list.end()) {
            segment_list.erase(it);
        }
    }
    catch(const std::exception& e)
    {
        return false;
    }
    return true;
}

#endif // TM_CPP