#pragma once
#include <functional>

template<typename F, typename... Args>
auto func_wrapper(F&& f, Args&&... args)
-> std::function<decltype(std::forward<F>(f)(std::forward<Args>(args)...))()> {
    using return_type = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
    std::function<return_type()> binded_func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    return binded_func();
}