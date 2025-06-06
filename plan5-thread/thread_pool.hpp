#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <vector>

template<typename T>
class safe_queue {
private:
    std::mutex m_Mutex;
    std::queue<T> m_Queue;
public:
    safe_queue() = default;
    ~safe_queue() = default;
    safe_queue(const safe_queue&) = delete;
    safe_queue& operator=(const safe_queue&) = delete;

    bool empty() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        return m_Queue.empty();
    }

    int size() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        return m_Queue.size();
    }

    void push(const T& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Queue.push(value);
    }

    void push(T&& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Queue.push(std::move(value));
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        if (m_Queue.empty()) {
            return false;
        }
        value = std::move(m_Queue.front());
        m_Queue.pop();
        return true;
    }

    void clear() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        while (!m_Queue.empty()) {
            m_Queue.pop();
        }
    }
};

class thread_pool {
private:
    /* ------------------------------------------- */
    class thread_worker {
    private:
        thread_pool* pool_ptr;
        int m_Id;
        std::thread m_Thread;

    public:
        bool isWorking;
        bool terminate;

        thread_worker(thread_pool* pool, int id) : pool_ptr(pool), m_Id(id) {}

        void operator()() {
            while (pool_ptr->pool_status <= 1) {
                std::function<void()> func;
                {
                    std::unique_lock<std::mutex> lock(pool_ptr->m_Mutex);
                    pool_ptr->m_Condition.wait(lock, [&]{
                        return pool_ptr->pool_status == 1 || !pool_ptr->m_TaskQueue.empty();
                    });
                    if (pool_ptr->pool_status == 1 && pool_ptr->m_TaskQueue.empty())
                        return;
                    if (!pool_ptr->m_TaskQueue.pop(func))
                        continue;
                }
                func();
            }
        }
    };
    /* ------------------------------------------- */
    safe_queue<std::function<void()>> m_TaskQueue;
    std::vector<std::thread> m_Workers;
public:
    std::mutex m_Mutex;
    std::condition_variable m_Condition;
    int pool_core_size;
    int pool_max_size;
    int pool_status = 0; // 0: working, 1: shutdown, 2: stop, 3: tidying, 4: terminated
    bool is_shutting_down = false;

    thread_pool(int core_size = 4, int max_size = 8) : pool_core_size(core_size), pool_max_size(max_size) {}

    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    thread_pool& operator=(thread_pool&&) = delete;

    ~thread_pool() {
        stop();
        tidy();
    }

    void init() {
        pool_status = 0;
        for (int i = 0; i < pool_core_size; ++i) {
            m_Workers.push_back(std::thread(thread_worker(this, i)));
        }
    }

    void shutdown() {
        pool_status = 1;
        m_Condition.notify_all();
        for (auto& worker : m_Workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        tidy();
    }

    void stop() {
        pool_status = 2;
        for (auto& worker : m_Workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        tidy();
    }

    void tidy() {
        pool_status = 3;
        m_Workers.clear();
        m_TaskQueue.clear();
    }

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))> {
        using return_type = decltype(std::forward<F>(f)(std::forward<Args>(args)...)); // C++就是1to4
        std::function<return_type()> task_func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(task_func);
        std::function<void()> l_func = [task_ptr]() {
            (*task_ptr)();
        };
        if (pool_status != 0) {
            return std::future<return_type>();
        }
        m_TaskQueue.push(l_func);
        m_Condition.notify_one();
        return task_ptr->get_future();
    }
};

#endif // THREAD_POOL_HPP