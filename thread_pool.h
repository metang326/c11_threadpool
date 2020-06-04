#ifndef THREADPOOL_THREAD_POOL_H
#define THREADPOOL_THREAD_POOL_H

#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>
#include <chrono>
#include <map>
#include <map>
#include <sstream>

using namespace std;
typedef function<int()> Task;

int print(int task_id) {
    cout << "[-] task " << task_id << " working in thread " << this_thread::get_id() << endl;
    return task_id;
}

class ThreadPool {
private:
    int thread_num;
    int max_task_num;
    bool is_working;
    condition_variable cond{};
    vector<thread> threads{};
    queue<Task> tasks;
    mutex task_mutex{};
    map<int, string> count;

public:
    void work() {
        while (this->is_working) {
            ostringstream ss;
            ss << this_thread::get_id();
            string cur_thread_id = ss.str();
            unique_lock<mutex> lk(this->task_mutex);
            while (this->is_working and this->tasks.empty()) {
                this->cond.wait(lk);
            }
            if (this->tasks.empty()) {
                continue;
            }
            Task cur_task;
            cur_task = this->tasks.front();
            this->tasks.pop();
            int task_id = cur_task();
            this->count[task_id] = cur_thread_id;
            this->cond.notify_one();
        }
    }

    ThreadPool(int thread_num, int max_task_num) {
        this->thread_num = thread_num;
        this->max_task_num = max_task_num;
        this->is_working = true;
        for (int i = 0; i < thread_num; i++) {
            this->threads.push_back(thread(&ThreadPool::work, this));
        }
        cout << "[✔] create ThreadPool finished!" << endl;
    }

    ~ThreadPool() {
        this->is_working = false;
        this->cond.notify_all();
        cout << "------------delete pool--------------" << endl;
        for (auto &t:this->threads) {
            if (t.joinable()) {
                cout << "[-] join thread " << t.get_id();
                ostringstream ss;
                ss << t.get_id();
                string cur_thread_id = ss.str();
                int task_num = 0;
                for (auto &dict:count) {
                    if (dict.second == cur_thread_id) {
                        task_num++;
                    }
                }
                cout << ", finished task_num =" << task_num << endl;
                t.join();

            }
        }
        cout << "[✔] delete ThreadPool finished!" << endl;
    }

    void add_task(const Task &t) {
        if (this->is_working) {
            unique_lock<mutex> lk(this->task_mutex);
            while (tasks.size() >= this->max_task_num) {
                this->cond.wait(lk);
            }
            this->tasks.push(t);
            this->cond.notify_all();
        }
    }
};


#endif //THREADPOOL_THREAD_POOL_H
