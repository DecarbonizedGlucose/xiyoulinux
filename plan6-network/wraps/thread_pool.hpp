/*
    万用线程池 C++11泛型编程
*/

#pragma once
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
    safe_queue(safe_queue&&) = delete;
    safe_queue& operator=(safe_queue&&) = delete;
    bool empty();
    int size();
    void push(T& value);
    bool pop(T& value);
    void clear();
};

template<typename T>
bool safe_queue<T>::empty() {
    std::unique_lock<std::mutex> lock(m_Mutex);
    return m_Queue.empty();
}

template<typename T>
int safe_queue<T>::size() {
    std::unique_lock<std::mutex> lock(m_Mutex);
    return m_Queue.size();
}

template<typename T>
void safe_queue<T>::push(T& value) {
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_Queue.push(value);
}

template<typename T>
bool safe_queue<T>::pop(T& value) {
    std::unique_lock<std::mutex> lock(m_Mutex);
    if (m_Queue.empty()) {
        return false;
    }
    value = std::move(m_Queue.front());
    m_Queue.pop();
    return true;
}

template<typename T>
void safe_queue<T>::clear() {
    std::unique_lock<std::mutex> lock(m_Mutex);
    while (!m_Queue.empty()) {
        m_Queue.pop();
    }
}

class thread_pool {
private:
    class thread_worker;
    safe_queue<std::function<void()>> m_TaskQueue;
    std::vector<std::thread> m_Workers;
public:
    std::mutex m_Mutex;
    std::condition_variable m_Condition;
    int pool_core_size;
    int pool_max_size;
    int pool_status = 0; // 0: working, 1: shutdown, 2: stop, 3: tidying, 4: terminated
    bool is_shutting_down = false;

    thread_pool() = delete;
    thread_pool(int core_size = 4, int max_size = 8)
    : pool_core_size(core_size), pool_max_size(max_size) {}
    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    thread_pool& operator=(thread_pool&&) = delete;
    ~thread_pool();
    void init();
    void shutdown();
    void stop();
    void tidy();

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>;
};

class thread_pool::thread_worker {
private:
    thread_pool* pool_ptr;
    int m_Id;
public:
    bool is_working;
    bool terminate;
    thread_worker(thread_pool* pool, int id)
    : pool_ptr(pool), m_Id(id), is_working(false), terminate(false) {}
    void operator()();
};

void thread_pool::thread_worker::operator()() {
    while (pool_ptr->pool_status <= 1) {
        std::function<void()> func;
        bool got_task;
        { // 这里把锁和执行关在一起，粒度小
            std::unique_lock<std::mutex> lock(pool_ptr->m_Mutex);
            if (pool_ptr->m_TaskQueue.empty()) {
                if (pool_ptr->pool_status == 1) {
                    return; // 如果线程池正在关闭，直接返回
                }
                pool_ptr->m_Condition.wait(lock); // 等待任务到来
            }
            got_task = pool_ptr->m_TaskQueue.pop(func);
            if (got_task) {
                func(); // 执行任务
            }
        }
    }
}

thread_pool::~thread_pool() {
    stop();
    tidy();
}

void thread_pool::init() {
    if (pool_status != 0) {
        return; // 如果线程池已经在工作或已关闭，直接返回
    }
    pool_status = 0;
    for (int i = 0; i < pool_core_size; ++i) {
        m_Workers.push_back(std::thread(thread_worker(this, i)));
    }
}

void thread_pool::shutdown() {
    if (pool_status != 0) {
        return; // 如果线程池已经在工作或已关闭，直接返回
    }
    pool_status = 1; // 设置为关闭状态
    is_shutting_down = true;
    m_Condition.notify_all(); // 通知所有线程
    for (auto& worker : m_Workers) {
        if (worker.joinable()) {
            worker.join(); // 等待所有线程结束
        }
    }
    tidy();
}

void thread_pool::stop() {
    pool_status = 2;
    for (auto& worker : m_Workers) {
        if (worker.joinable()) {
            worker.join(); // 等待所有线程结束
        }
    }
    tidy();
}

void thread_pool::tidy() {
    pool_status = 3;
    m_Workers.clear();
    m_TaskQueue.clear();
}

template<typename F, typename... Args>
auto thread_pool::submit(F&& f, Args&&... args) -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))> {
    using return_type = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
    std::function<return_type()> task_func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(task_func);
    std::function<void()> l_func = [task_ptr]() { // 这里push的是一个labmda，类型其实是std::function<void()>
        (*task_ptr)();
    };
    if (pool_status != 0) {
        return std::future<return_type>(); // 线程池已经关闭，返回一个空的future
    }
    m_TaskQueue.push(l_func);
    m_Condition.notify_one();
    return task_ptr->get_future(); // 返回先前注册的任务指针
}