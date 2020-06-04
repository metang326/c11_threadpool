#include <chrono>
#include "thread_pool.h"

int main(int argc, char *argv[]) {
    ThreadPool *thread_pool = new ThreadPool(10, 100);
    for (int i = 0; i < 5000; i++) {
        thread_pool->add_task([=] { return print(i); });
    }
    this_thread::sleep_for(chrono::milliseconds(2000));
    delete thread_pool;
    return 0;
}