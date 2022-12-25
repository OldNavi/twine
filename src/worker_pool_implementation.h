/*
 * Copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk
 * Twine is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * Twine is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with Twine.
 * If not, see http://www.gnu.org/licenses/ .
 */

/**
 * @brief Worker pool implementation
 * @copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef TWINE_WORKER_POOL_IMPLEMENTATION_H
#define TWINE_WORKER_POOL_IMPLEMENTATION_H

#include <cassert>
#include <atomic>
#include <vector>
#include <array>
#include <cstring>
#include <cerrno>
#include <optional>
#include <string>
#ifdef TWINE_BUILD_WITH_XENOMAI
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <evl/mutex.h>
#include <evl/event.h>
#endif
#include <stdexcept>

#include "thread_helpers.h"
#include "twine_internal.h"

namespace twine {

void set_flush_denormals_to_zero();
inline void enable_break_on_mode_sw()
{
    pthread_setmode_np(0, PTHREAD_WARNSW, 0);
}
#ifdef TWINE_BUILD_WITH_XENOMAI
int ConvertString2Int(const std::string& str)
{
    std::stringstream ss(str);
    int x;
    if (! (ss >> x))
    {
        std::cerr << "Error converting " << str << " to integer" << std::endl;
        abort();
    }
    return x;
}

std::vector<std::string> SplitStringToArray(const std::string& str, char splitter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string temp;
    while (getline(ss, temp, splitter)) // split into new "lines" based on character
    {
        tokens.push_back(temp);
    }
    return tokens;
}

std::vector<int> ParseData(const std::string& data)
{
    std::vector<std::string> tokens = SplitStringToArray(data, ',');

    std::vector<int> result;
    for (std::vector<std::string>::const_iterator it = tokens.begin(), end_it = tokens.end(); it != end_it; ++it)
    {
        const std::string& token = *it;
        std::vector<std::string> range = SplitStringToArray(token, '-');
        if (range.size() == 1)
        {
            result.push_back(ConvertString2Int(range[0]));
        }
        else if (range.size() == 2)
        {
            int start = ConvertString2Int(range[0]);
            int stop = ConvertString2Int(range[1]);
            for (int i = start; i <= stop; i++)
            {
                result.push_back(i);
            }
        }
        else
        {
            std::cerr << "Error parsing token " << token << std::endl;
            abort();
        }
    }

    return result;
}
#endif

inline WorkerPoolStatus errno_to_worker_status(int error)
{
    switch (error)
    {
        case 0:
            return WorkerPoolStatus::OK;

        case EAGAIN:
            return WorkerPoolStatus::LIMIT_EXCEEDED;

        case EPERM:
            return WorkerPoolStatus::PERMISSION_DENIED;

        case EINVAL:
            return WorkerPoolStatus::INVALID_ARGUMENTS;

        default:
            return WorkerPoolStatus::ERROR;
    }
}

// /**
//  * @brief Thread barrier that can be controlled from an external thread
//  */
// template <ThreadType type>
// class BarrierWithTrigger
// {
// public:
//     TWINE_DECLARE_NON_COPYABLE(BarrierWithTrigger);
//     /**
//      * @brief Multithread barrier with trigger functionality
//      */
//     BarrierWithTrigger()
//     {
//         if constexpr (type == ThreadType::XENOMAI)
//         {
//             _evl_semaphores[0] = &_evl_semaphore_store[0];
//             _evl_semaphores[1] = &_evl_semaphore_store[1];
//             _evl_calling_mutex = EVL_MUTEX_INITIALIZER("mtx_calling", EVL_CLOCK_MONOTONIC, 0, EVL_MUTEX_NORMAL | EVL_CLONE_PUBLIC);
//             _evl_calling_cond = EVL_EVENT_INITIALIZER("evt_calling",  EVL_CLOCK_MONOTONIC, EVL_CLONE_PUBLIC);
//             int fd =   evl_create_sem(_evl_semaphores[0], EVL_CLOCK_MONOTONIC, 0, EVL_CLONE_PUBLIC, "twine_semaphore_0");
//             if(fd<0){
//                 std::cerr << "Cannot create semafore with error " << fd << std::endl;
//             }
//             fd = evl_create_sem(_evl_semaphores[1], EVL_CLOCK_MONOTONIC, 0, EVL_CLONE_PUBLIC, "twine_semaphore_1");
//             if(fd<0){
//                 std::cerr << "Cannot create semafore with error " << fd <<  std::endl;
//             }
//             _evl_active_sem = _evl_semaphores[0];
//         } else
//         {
//         mutex_create<type>(&_calling_mutex, nullptr);
//         condition_var_create<type>(&_calling_cond, nullptr);
//         int res = semaphore_create<type>(&_semaphores[0], "twine_semaphore_0");
//         if (res != 0)
//         {
//             throw std::runtime_error(strerror(res));
//         }
//         res = semaphore_create<type>(&_semaphores[1], "twine_semaphore_1");
//         if (res != 0)
//         {
//             throw std::runtime_error(strerror(res));
//         }
//         _active_sem = _semaphores[0];
//         }
//     }

//     /**
//      * @brief Destroy the barrier object
//      */
//     ~BarrierWithTrigger()
//     {
//         if constexpr (type == ThreadType::XENOMAI)
//         {
//             evl_close_mutex(&_evl_calling_mutex);
//             evl_close_event(&_evl_calling_cond);
//             evl_close_sem(_evl_semaphores[0]);
//             evl_close_sem(_evl_semaphores[1]);
//         }
//         else {
//         mutex_destroy<type>(&_calling_mutex);
//         condition_var_destroy<type>(&_calling_cond);
//         semaphore_destroy<type>(_semaphores[0], "twine_semaphore_0");
//         semaphore_destroy<type>(_semaphores[1], "twine_semaphore_1");
//         }
//     }

//     /**
//      * @brief Wait for signal to finish, called from threads participating on the
//      *        barrier
//      */
//     void wait()
//     {
//         if constexpr (type == ThreadType::XENOMAI)
//         {
//         evl_lock_mutex(&_evl_calling_mutex);
//         auto active_sem = _evl_active_sem;
//         if (++_no_threads_currently_on_barrier >= _no_threads)
//         {
//             evl_signal_event(&_evl_calling_cond);
//         }
//         evl_unlock_mutex(&_evl_calling_mutex);
//         evl_get_sem(active_sem);
//         }
//         else {
//         mutex_lock<type>(&_calling_mutex);
//         auto active_sem = _active_sem;
//         if (++_no_threads_currently_on_barrier >= _no_threads)
//         {
//             condition_signal<type>(&_calling_cond);
//         }
//         mutex_unlock<type>(&_calling_mutex);

//         semaphore_wait<type>(active_sem);
//         }
//     }

//     /**
//      * @brief Wait for all threads to halt on the barrier, called from a thread
//      *        not waiting on the barrier and will block until all threads are
//      *        waiting on the barrier.
//      */
//     void wait_for_all()
//     {
//         if constexpr (type == ThreadType::XENOMAI)
//         {
//             evl_lock_mutex(&_evl_calling_mutex);
//         int current_threads = _no_threads_currently_on_barrier;

//         if (current_threads == _no_threads)
//         {
//             evl_unlock_mutex(&_evl_calling_mutex);
//             return;
//         }
//         while (current_threads < _no_threads)
//         {
//             evl_wait_event(&_evl_calling_cond, &_evl_calling_mutex);
//             current_threads = _no_threads_currently_on_barrier;
//         }
//         evl_unlock_mutex(&_evl_calling_mutex);
//         }
//         else {
//         mutex_lock<type>(&_calling_mutex);
//         int current_threads = _no_threads_currently_on_barrier;

//         if (current_threads == _no_threads)
//         {
//             mutex_unlock<type>(&_calling_mutex);
//             return;
//         }
//         while (current_threads < _no_threads)
//         {
//             condition_wait<type>(&_calling_cond, &_calling_mutex);
//             current_threads = _no_threads_currently_on_barrier;
//         }
//         mutex_unlock<type>(&_calling_mutex);
//         }
//     }

//     /**
//      * @brief Change the number of threads for the barrier to handle.
//      * @param threads
//      */
//     void set_no_threads(int threads)
//     {
//         if constexpr (type == ThreadType::XENOMAI)
//         {
//          evl_lock_mutex(&_evl_calling_mutex);
//         _no_threads = threads;
//         evl_unlock_mutex(&_evl_calling_mutex);
//         }
//         else {
//         mutex_lock<type>(&_calling_mutex);
//         _no_threads = threads;
//         mutex_unlock<type>(&_calling_mutex);
//         }
//     }

//     /**
//      * @brief Release all threads waiting on the barrier.
//      */
//     void release_all()
//     {
//         if constexpr (type == ThreadType::XENOMAI)
//         {
//         evl_lock_mutex(&_evl_calling_mutex);

//         assert(_no_threads_currently_on_barrier == _no_threads);
//         _no_threads_currently_on_barrier = 0;

//         auto prev_sem = _evl_active_sem;
//         _swap_semaphores();

//         for (int i = 0; i < _no_threads; ++i)
//         {
//             evl_put_sem(prev_sem);
//         }

//         evl_unlock_mutex(&_evl_calling_mutex);
//         }
//         else {
//         mutex_lock<type>(&_calling_mutex);

//         assert(_no_threads_currently_on_barrier == _no_threads);
//         _no_threads_currently_on_barrier = 0;

//         auto prev_sem = _active_sem;
//         _swap_semaphores();

//         for (int i = 0; i < _no_threads; ++i)
//         {
//             semaphore_signal<type>(prev_sem);
//         }

//         mutex_unlock<type>(&_calling_mutex);
//         }
//     }

//     void release_and_wait()
//     {
//         if constexpr (type == ThreadType::XENOMAI)
//         {
//         evl_lock_mutex(&_evl_calling_mutex);
//         assert(_no_threads_currently_on_barrier == _no_threads);
//         _no_threads_currently_on_barrier = 0;

//         auto prev_sem = _evl_active_sem;
//         _swap_semaphores();

//         for (int i = 0; i < _no_threads; ++i)
//         {
//              evl_put_sem(prev_sem);
//         }

//         int current_threads = _no_threads_currently_on_barrier;

//         while (current_threads < _no_threads)
//         {
//             evl_wait_event(&_evl_calling_cond, &_evl_calling_mutex);
//             current_threads = _no_threads_currently_on_barrier;
//         }
//          evl_unlock_mutex(&_evl_calling_mutex);
//         }
//         else {
//         mutex_lock<type>(&_calling_mutex);
//         assert(_no_threads_currently_on_barrier == _no_threads);
//         _no_threads_currently_on_barrier = 0;

//         auto prev_sem = _active_sem;
//         _swap_semaphores();

//         for (int i = 0; i < _no_threads; ++i)
//         {
//             semaphore_signal<type>(prev_sem);
//         }

//         int current_threads = _no_threads_currently_on_barrier;

//         while (current_threads < _no_threads)
//         {
//             condition_wait<type>(&_calling_cond, &_calling_mutex);
//             current_threads = _no_threads_currently_on_barrier;
//         }
//         mutex_unlock<type>(&_calling_mutex);
//         }
//     }

// private:
//     void _swap_semaphores()
//     {
//         if constexpr (type == ThreadType::XENOMAI)
//         {
//         if (_evl_active_sem == _evl_semaphores[0])
//         {
//             _evl_active_sem = _evl_semaphores[1];
//         }
//         else
//         {
//             _evl_active_sem = _evl_semaphores[0];
//         }
//         }
//         else
//         {
//         if (_active_sem == _semaphores[0])
//         {
//             _active_sem = _semaphores[1];
//         }
//         else
//         {
//             _active_sem = _semaphores[0];
//         }
//         }
//     }

//     std::array<sem_t, 2 > _semaphore_store;
//     std::array<sem_t*, 2> _semaphores;
//     sem_t* _active_sem;

//     pthread_mutex_t _calling_mutex;
//     pthread_cond_t  _calling_cond;
// #ifdef TWINE_BUILD_WITH_XENOMAI
//     std::array<struct evl_sem, 2 > _evl_semaphore_store;
//     std::array<struct evl_sem*, 2> _evl_semaphores;
//     struct evl_sem* _evl_active_sem;

//     struct evl_mutex _evl_calling_mutex;
//     struct evl_event _evl_calling_cond;
// #endif

//     std::atomic<int> _no_threads_currently_on_barrier{0};
//     std::atomic<int> _no_threads{0};
// };


/**
 * @brief Thread barrier that can be controlled from an external thread
 */
template <ThreadType type>
class BarrierWithTrigger
{
public:
    TWINE_DECLARE_NON_COPYABLE(BarrierWithTrigger);
    /**
     * @brief Multithread barrier with trigger functionality
     */
    BarrierWithTrigger()
    {
        if constexpr (type == ThreadType::XENOMAI)
        {

            _evl_calling_mutex = EVL_MUTEX_INITIALIZER("mtx_calling", EVL_CLOCK_MONOTONIC, 0, EVL_MUTEX_NORMAL | EVL_CLONE_PUBLIC);
            _evl_thread_mutex = EVL_MUTEX_INITIALIZER("mtx_thread", EVL_CLOCK_MONOTONIC, 0, EVL_MUTEX_NORMAL | EVL_CLONE_PUBLIC);
            _evl_calling_cond = EVL_EVENT_INITIALIZER("evt_calling",  EVL_CLOCK_MONOTONIC, EVL_CLONE_PUBLIC);
            _evl_thread_cond = EVL_EVENT_INITIALIZER("evt_thread",  EVL_CLOCK_MONOTONIC, EVL_CLONE_PUBLIC);

        } else {
        mutex_create<type>(&_thread_mutex, nullptr);
        mutex_create<type>(&_calling_mutex, nullptr);
        condition_var_create<type>(&_thread_cond, nullptr);
        condition_var_create<type>(&_calling_cond, nullptr);
        }
    }

    /**
     * @brief Destroy the barrier object
     */
    ~BarrierWithTrigger()
    {
        if constexpr (type == ThreadType::XENOMAI)
        {
            evl_close_mutex(&_evl_calling_mutex);
            evl_close_event(&_evl_calling_cond);
            evl_close_mutex(&_evl_thread_mutex);
            evl_close_event(&_evl_thread_cond);

        }
        else {
        mutex_destroy<type>(&_thread_mutex);
        mutex_destroy<type>(&_calling_mutex);
        condition_var_destroy<type>(&_thread_cond);
        condition_var_destroy<type>(&_calling_cond);
        }
    }

    /**
     * @brief Wait for signal to finish, called from threads participating on the
     *        barrier
     */
    void wait()
    {
        const bool& halt_flag = *_halt_flag; // 'local' halt flag for this round
        if constexpr (type == ThreadType::XENOMAI)
        {
        evl_lock_mutex(&_evl_calling_mutex);
        if (++_no_threads_currently_on_barrier >= _no_threads)
        {
            evl_signal_event(&_evl_calling_cond);
        }
        evl_unlock_mutex(&_evl_calling_mutex);

        evl_lock_mutex(&_evl_thread_mutex);
        while (halt_flag)
        {
            // The condition needs to be rechecked when waking as threads may wake up spuriously
            evl_wait_event(&_evl_thread_cond, &_evl_thread_mutex);
        }
        evl_unlock_mutex(&_evl_thread_mutex);
        }
        else
        {
        mutex_lock<type>(&_calling_mutex);
        if (++_no_threads_currently_on_barrier >= _no_threads)
        {
            condition_signal<type>(&_calling_cond);
        }
        mutex_unlock<type>(&_calling_mutex);

        mutex_lock<type>(&_thread_mutex);
        while (halt_flag)
        {
            // The condition needs to be rechecked when waking as threads may wake up spuriously
            condition_wait<type>(&_thread_cond, &_thread_mutex);
        }
        mutex_unlock<type>(&_thread_mutex);
        }
    }

    /**
     * @brief Wait for all threads to halt on the barrier, called from a thread
     *        not waiting on the barrier and will block until all threads are
     *        waiting on the barrier.
     */
    void wait_for_all()
    {
        if constexpr (type == ThreadType::XENOMAI)
        {
        evl_lock_mutex(&_evl_calling_mutex);
        int current_threads = _no_threads_currently_on_barrier;
        if (current_threads == _no_threads)
        {
            evl_unlock_mutex(&_evl_calling_mutex);
            return;
        }
        while (current_threads < _no_threads)
        {
            evl_wait_event(&_evl_calling_cond, &_evl_calling_mutex);
            current_threads = _no_threads_currently_on_barrier;
        }
        evl_unlock_mutex(&_evl_calling_mutex);
        }
        else {
        mutex_lock<type>(&_calling_mutex);
        int current_threads = _no_threads_currently_on_barrier;
        if (current_threads == _no_threads)
        {
            mutex_unlock<type>(&_calling_mutex);
            return;
        }
        while (current_threads < _no_threads)
        {
            condition_wait<type>(&_calling_cond, &_calling_mutex);
            current_threads = _no_threads_currently_on_barrier;
        }
        mutex_unlock<type>(&_calling_mutex);
        }
    }

    /**
     * @brief Change the number of threads for the barrier to handle.
     * @param threads
     */
    void set_no_threads(int threads)
    {
        if constexpr (type == ThreadType::XENOMAI)
        {
        evl_lock_mutex(&_evl_calling_mutex);
        _no_threads = threads;
        evl_unlock_mutex(&_evl_calling_mutex);

        }
        else {
        mutex_lock<type>(&_calling_mutex);
        _no_threads = threads;
        mutex_unlock<type>(&_calling_mutex);
        }
    }

    /**
     * @brief Release all threads waiting on the barrier.
     */
    void release_all()
    {
        if constexpr (type == ThreadType::XENOMAI)
        {
        evl_lock_mutex(&_evl_calling_mutex);
        assert(_no_threads_currently_on_barrier == _no_threads);
        _swap_halt_flags();
        _no_threads_currently_on_barrier = 0;
        /* For xenomai threads, it is neccesary to hold the mutex while
         * sending the broadcast. Otherwise deadlocks can occur. For
         * pthreads it is not neccesary but it is recommended for
         * good realtime performance. And surprisingly enough seems
         * a bit faster that without holding the mutex.  */
        evl_lock_mutex(&_evl_thread_mutex);
        evl_broadcast_event(&_evl_thread_cond);
        evl_unlock_mutex(&_evl_thread_mutex);
        evl_unlock_mutex(&_evl_calling_mutex);
        }
        else {
        mutex_lock<type>(&_calling_mutex);
        assert(_no_threads_currently_on_barrier == _no_threads);
        _swap_halt_flags();
        _no_threads_currently_on_barrier = 0;
        /* For xenomai threads, it is neccesary to hold the mutex while
         * sending the broadcast. Otherwise deadlocks can occur. For
         * pthreads it is not neccesary but it is recommended for
         * good realtime performance. And surprisingly enough seems
         * a bit faster that without holding the mutex.  */
        mutex_lock<type>(&_thread_mutex);
        condition_broadcast<type>(&_thread_cond);
        mutex_unlock<type>(&_thread_mutex);
        mutex_unlock<type>(&_calling_mutex);
        }
    }


private:
    void _swap_halt_flags()
    {
        *_halt_flag = false;
        if (_halt_flag == &_halt_flags[0])
        {
            _halt_flag = &_halt_flag[1];
        }
        else
        {
            _halt_flag = &_halt_flags[0];
        }
        *_halt_flag = true;
    }

    pthread_mutex_t _thread_mutex;
    pthread_mutex_t _calling_mutex;

    pthread_cond_t _thread_cond;
    pthread_cond_t _calling_cond;
#ifdef TWINE_BUILD_WITH_XENOMAI
    struct evl_mutex _evl_calling_mutex;
    struct evl_event _evl_calling_cond;
    struct evl_mutex _evl_thread_mutex;
    struct evl_event _evl_thread_cond;
#endif

    std::array<bool, 2> _halt_flags{true, true};
    bool*_halt_flag{&_halt_flags[0]};
    int _no_threads_currently_on_barrier{0};
    int _no_threads{0};
};

template <ThreadType type>
class WorkerThread
{
public:
    TWINE_DECLARE_NON_COPYABLE(WorkerThread);

    WorkerThread(BarrierWithTrigger<type>& barrier, WorkerCallback callback,
                                         void*callback_data, std::atomic_bool& running_flag,
                                         bool disable_denormals,
                                         bool break_on_mode_sw): _barrier(barrier),
                                                                  _callback(callback),
                                                                  _callback_data(callback_data),
                                                                  _running(running_flag),
                                                                  _disable_denormals(disable_denormals),
                                                                  _break_on_mode_sw(break_on_mode_sw)

    {}

    ~WorkerThread()
    {
        if (_thread_handle != 0)
        {
#ifdef TWINE_BUILD_WITH_XENOMAI
        evl_detach_self();
#endif
            pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,nullptr);
            pthread_cancel(_thread_handle);
            // thread_join<type>(_thread_handle, nullptr);
        }
    }

    int run(int sched_priority, [[maybe_unused]] int cpu_id)
    {
        if ( (sched_priority < 0) || (sched_priority > 100) )
        {
            return EINVAL;
        }
        _priority = sched_priority;
        struct sched_param rt_params = {.sched_priority = sched_priority};
        pthread_attr_t task_attributes;
        pthread_attr_init(&task_attributes);

        pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
        pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
        pthread_attr_setschedparam(&task_attributes, &rt_params);
        auto res = 0;
#ifndef __APPLE__
        cpu_set_t cpus;
        CPU_ZERO(&cpus);
        CPU_SET(cpu_id, &cpus);
        res = pthread_attr_setaffinity_np(&task_attributes, sizeof(cpu_set_t), &cpus);
#endif
        if (res == 0)
        {
            res = thread_create<type>(&_thread_handle, &task_attributes, &_worker_function, this);
        }
        pthread_attr_destroy(&task_attributes);
        return res;
    }

    static void* _worker_function(void* data)
    {
        #ifdef TWINE_BUILD_WITH_XENOMAI
        evl_attach_self("/worker_thread:%d",pthread_self());
        #endif
        reinterpret_cast<WorkerThread<type>*>(data)->_internal_worker_function();
        return nullptr;
    }

private:
    void _internal_worker_function()
    {
        // Signal that this is a realtime thread
        ThreadRtFlag rt_flag;
        if (_disable_denormals)
        {
            set_flush_denormals_to_zero();
        }
        if (type == ThreadType::XENOMAI && _break_on_mode_sw)
        {
            enable_break_on_mode_sw();
        }

        while (true)
        {
            _barrier.wait();
            if (_running.load() == false)
            {
                // condition checked when coming out of wait as we might want to exit immediately here
                break;
            }
            _callback(_callback_data);
        }
    }

    BarrierWithTrigger<type>&   _barrier;
    pthread_t                   _thread_handle{0};
    WorkerCallback              _callback;
    void*                       _callback_data;
    const std::atomic_bool&     _running;
    bool                        _disable_denormals;
    int                         _priority {0};
    bool                        _break_on_mode_sw;
};

template <ThreadType type>
class WorkerPoolImpl : public WorkerPool
{
public:
    TWINE_DECLARE_NON_COPYABLE(WorkerPoolImpl);

    explicit WorkerPoolImpl(int cores,
                            bool disable_denormals) : _no_cores(cores),
                                                     _cores_usage(cores, 0),
                                                     _disable_denormals(disable_denormals)

    {
    #ifdef TWINE_BUILD_WITH_XENOMAI
    std::ifstream input("/sys/devices/system/cpu/isolated");
    std::string line;
    std::getline( input, line );
	core_numbers = ParseData(line);
    if(core_numbers.size()==0)
    {
        std::cerr << "Need to have isolated cpus setup in kernel" << std::endl;
        abort();
    }
    if(core_numbers.size()<cores)
    {
        std::cerr << "Requested number should not exceed isolated cpus " << std::endl;
        abort();
    }
    // std::cerr << "Pool cores: ";

    for (std::vector<int>::const_iterator it = core_numbers.begin(), end_it = core_numbers.end(); it != end_it; ++it)
    {
        std::cerr << *it << " ";
    }
    std::cerr << std::endl;
#endif
    }

    ~WorkerPoolImpl()
    {
        _barrier.wait_for_all();
        _running.store(false);
        _barrier.release_all();
    }

    WorkerPoolStatus add_worker(WorkerCallback worker_cb, void* worker_data,
                                int sched_priority=75,
                                std::optional<int> cpu_id=std::nullopt) override    {
        int core = 0;
        if (cpu_id.has_value())
        {
            core = cpu_id.value();
            if ( (core < 0) || (core >= _no_cores) )
            {
                return WorkerPoolStatus::INVALID_ARGUMENTS;
            }
        }
        {
            // If no core is specified, pick the first core with least usage
            int min_idx = _no_cores - 1;
            int min_usage = _cores_usage[min_idx];
            for (int n = _no_cores-1; n >= 0; n--)
            {
                int cur_usage = _cores_usage[n];
                if (cur_usage <= min_usage)
                {
                    min_usage = cur_usage;
                    min_idx = n;
                }
            }
            core = min_idx;
        }
        // round-robin assignment to cpu cores
        _cores_usage[core]++;
#ifdef TWINE_BUILD_WITH_XENOMAI
        core = core_numbers[core];
        //std::cerr << "Core assigned = " << core << std::endl;
#endif

        auto worker = std::make_unique<WorkerThread<type>>(_barrier, worker_cb, worker_data, _running,
                                                           _disable_denormals);
        _barrier.set_no_threads(_no_workers + 1);
        auto res = errno_to_worker_status(worker->run(sched_priority, core));
        if (res == WorkerPoolStatus::OK)
        {
            // Wait until the thread is idle to avoid synchronisation issues
            _no_workers++;
            _workers.push_back(std::move(worker));
            _barrier.wait_for_all();
        }
        else
        {
            _barrier.set_no_threads(_no_workers);
        }
        return res;
    }

    void wait_for_workers_idle() override
    {
        _barrier.wait_for_all();
    }

    void wakeup_workers() override
    {
        _barrier.release_all();
    }



private:
    std::atomic_bool            _running{true};
    int                         _no_workers{0};
    int                         _no_cores;
    std::vector<int>            _cores_usage;
    bool                        _disable_denormals;
    bool                        _break_on_mode_sw;
    BarrierWithTrigger<type>    _barrier;
    std::vector<std::unique_ptr<WorkerThread<type>>> _workers;
#ifdef TWINE_BUILD_WITH_XENOMAI
    std::vector<int> core_numbers;
#endif
};


}// namespace twine



#endif //TWINE_WORKER_POOL_IMPLEMENTATION_H
