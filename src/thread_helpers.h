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
 * @brief Templated helper functions to make the Workerpool implementation thread-agnostic.
 * @copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef TWINE_THREAD_HELPERS_H
#define TWINE_THREAD_HELPERS_H

#include <cassert>

#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#ifdef TWINE_BUILD_WITH_XENOMAI
#pragma GCC diagnostic ignored "-Wunused-parameter"
// #include <cobalt/pthread.h>
#include <sys/types.h>
#include <unistd.h>
#pragma GCC diagnostic pop
#include <evl/sem.h>
#include <evl/event.h>
#include <evl/mutex.h>
#endif
#ifndef TWINE_BUILD_WITH_XENOMAI
#include "xenomai_stubs.h"
#endif

#include "twine/twine.h"

namespace twine {

enum class ThreadType : uint32_t
{
    PTHREAD,
    XENOMAI
};

template<ThreadType type, typename T>
inline int mutex_create(T* mutex, const void* attributes)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, pthread_mutex_t> )
     {
        return pthread_mutex_init(mutex, (pthread_mutexattr_t*)attributes);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_mutex> )
     {
         return evl_create_mutex(mutex, EVL_CLOCK_MONOTONIC, 0, EVL_MUTEX_NORMAL|EVL_CLONE_PRIVATE, (const char *)attributes);
     }
#endif
return 0;
}

template<ThreadType type, typename T>
inline int mutex_destroy(T* mutex)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, pthread_mutex_t> )
     {
        return pthread_mutex_destroy(mutex);
        }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_mutex> )
     {
         return evl_close_mutex(mutex);
     }
#endif
return 0;
}

template<ThreadType type,typename T>
inline int mutex_lock(T* mutex)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, pthread_mutex_t> )
     {
        return pthread_mutex_lock(mutex);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_mutex>)
     {
         return evl_lock_mutex(mutex);
     }
#endif
return 0;
}

template<ThreadType type,typename T>
inline int mutex_unlock(T* mutex)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, pthread_mutex_t> )
     {
        return pthread_mutex_unlock(mutex);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_mutex>)
     {
         return evl_unlock_mutex(mutex);
     }
#endif
return 0;
}

template<ThreadType type,typename T>
inline int condition_var_create(T* condition_var, const void* attributes)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, pthread_cond_t> )
     {
        return pthread_cond_init(condition_var, (const pthread_condattr_t*)attributes);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_event>)
     {
         return evl_new_event(condition_var,(const char *)attributes);
     }
#endif
return 0;
}

template<ThreadType type,typename T>
inline int condition_var_destroy(T* condition_var)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, pthread_cond_t>)
     {
        return pthread_cond_destroy(condition_var);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_event>)
     {
         return evl_close_event(condition_var);
     }
#endif
return 0;
}

template<ThreadType type, typename T, typename M>
inline int condition_wait(T* condition_var, M* mutex)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, pthread_cond_t> && std::is_same_v<M, pthread_mutex_t>)
     {
        return pthread_cond_wait(condition_var, mutex);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_event> && std::is_same_v<M, struct evl_mutex> )
     {
         return evl_wait_event(condition_var,mutex);
     }
#endif
return 0;
}

template<ThreadType type,  typename T>
inline int condition_signal(T* condition_var)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, pthread_cond_t> )
     {
        return pthread_cond_signal(condition_var);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_event> )
     {
         return evl_signal_event(condition_var);
     }
#endif
return 0;
}

template<ThreadType type,typename T>
inline int condition_broadcast(T* condition_var)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, pthread_cond_t>)
     {
        return pthread_cond_broadcast(condition_var);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_event>)
     {
         return evl_broadcast_event(condition_var);
     }
#endif
return 0;
}

template<ThreadType type>
inline int thread_create(pthread_t* thread, const pthread_attr_t* attributes, void *(*entry_fun) (void *), void* argument)
{
    // if constexpr (type == ThreadType::PTHREAD)
    // {
    //     return pthread_create(thread, attributes, entry_fun, argument);
    // }
    // else if constexpr (type == ThreadType::XENOMAI)
    // {
        return  pthread_create(thread, attributes, entry_fun, argument);
    // }
}

template<ThreadType type>
inline int thread_join(pthread_t thread, void** return_var = nullptr)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return pthread_join(thread, return_var);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
        evl_detach_self();
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,nullptr);
        return pthread_cancel(thread);
    }
#endif
return 0;
}

template<ThreadType type,typename T>
inline int semaphore_create(T** semaphore, [[maybe_unused]] const char* semaphore_name)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, sem_t>)
     {
        sem_unlink(semaphore_name);
        *semaphore = sem_open(semaphore_name, O_CREAT, 0, 0);
        if (*semaphore == SEM_FAILED)
        {
            return errno;
        }
        return 0;
     }
  #ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI  && std::is_same_v<T, struct evl_sem>)
     {
     	return evl_create_sem(*semaphore, EVL_CLOCK_MONOTONIC, 0, EVL_CLONE_PRIVATE,semaphore_name);
     }
 #endif
return 0;
}

template<ThreadType type,typename T>
inline int semaphore_destroy(T* semaphore, [[maybe_unused]] const char* semaphore_name)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, sem_t>)
     {
        sem_unlink(semaphore_name);
        sem_close(semaphore);
        return 0;
     }
 #ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_sem>)
     {
         return evl_close_sem(semaphore);
     }
 #endif
return 0;
}

template<ThreadType type,typename T>
inline int semaphore_wait(T* semaphore)
{
     if constexpr (type == ThreadType::PTHREAD  && std::is_same_v<T, sem_t>)
     {
        return sem_wait(semaphore);
     }
 #ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI  && std::is_same_v<T, struct evl_sem>)
     {
         return evl_get_sem(semaphore);
     }
 #endif
return 0;
}

template<ThreadType type,typename T>
inline int semaphore_signal(T* semaphore)
{
     if constexpr (type == ThreadType::PTHREAD && std::is_same_v<T, sem_t>)
     {
        return sem_post(semaphore);
     }
 #ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI && std::is_same_v<T, struct evl_sem>)
     {
         return evl_put_sem(semaphore);
     }
 #endif
return 0;
}
} // namespace twine

#endif //TWINE_THREAD_HELPERS_H
