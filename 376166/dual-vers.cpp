#include "dual-vers.hpp"

std::atomic_int counter, remaining;
std::atomic<bool> sleep_task{false};
std::atomic<std::vector<pthread_t>> blocked_pthread;

void enter_epoch() {
    pthread_t current_pthread;
    if (remaining) {
        blocked_pthread.push(current_pthread);
        sleep_task.wait(false);
    }
    else {
        remaining = 1;
    }
}

void leave_epoch() {
    remaining --;
    if (!remaining) {
        counter ++;
        remaining = blocked_pthread.size();
        for (auto pthread_: blocked_pthread) {
            sleep_task.notify_all();
        }
        blocked_pthread.clear();
    }
    else {
        reamining = 1;
    }
}