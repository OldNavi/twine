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
     if constexpr (type == ThreadType::PTHREAD)
     {
        return pthread_mutex_init(mutex, (pthread_mutexattr_t*)attributes);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_create_mutex((mutex, EVL_CLOCK_MONOTONIC, 0, EVL_MUTEX_NORMAL|EVL_CLONE_PRIVATE, (const char *)attributes);
     }
#endif
}

template<ThreadType type>
inline int mutex_destroy(void* mutex)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return pthread_mutex_destroy((pthread_mutex_t*)mutex);
        }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_close_mutex((struct evl_mutex *)mutex);
     }
#endif
}

template<ThreadType type>
inline int mutex_lock(void* mutex)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return pthread_mutex_lock((pthread_mutex_t*)mutex);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_lock_mutex((struct evl_mutex *)mutex);
     }
#endif
}

template<ThreadType type>
inline int mutex_unlock(void* mutex)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return pthread_mutex_unlock((pthread_mutex_t*)mutex);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_unlock_mutex((struct evl_mutex *)mutex);
     }
#endif
}

template<ThreadType type>
inline int condition_var_create(void* condition_var, const void* attributes)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return pthread_cond_init((pthread_cond_t*)condition_var, (const pthread_condattr_t*)attributes);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_new_event((struct evl_event *)condition_var,(const char *)attributes);
     }
#endif
}

template<ThreadType type>
inline int condition_var_destroy(void* condition_var)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return pthread_cond_destroy((pthread_cond_t *)condition_var);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_close_event((struct evl_event *)condition_var);
     }
#endif
}

template<ThreadType type>
inline int condition_wait(void* condition_var, void* mutex)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return pthread_cond_wait((pthread_cond_t *)condition_var, (pthread_mutex_t *)mutex);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_wait_event((struct evl_event *)condition_var,(struct evl_mutex *)mutex);
     }
#endif
}

template<ThreadType type>
inline int condition_signal(void* condition_var)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return pthread_cond_signal((pthread_cond_t*)condition_var);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_signal_event((struct evl_event *)condition_var);
     }
#endif
}

template<ThreadType type>
inline int condition_broadcast(void* condition_var)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return pthread_cond_broadcast((pthread_cond_t*)condition_var);
     }
#ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_broadcast_event((struct evl_event *)condition_var);
     }
#endif
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
}

template<ThreadType type>
inline int semaphore_create(void** semaphore, [[maybe_unused]] const char* semaphore_name)
{
     if constexpr (type == ThreadType::PTHREAD)
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
     else if constexpr (type == ThreadType::XENOMAI)
     {
     	return evl_create_sem(*semaphore, EVL_CLOCK_MONOTONIC, 0, EVL_CLONE_PRIVATE,semaphore_name);
     }
 #endif
}

template<ThreadType type>
inline int semaphore_destroy(void* semaphore, [[maybe_unused]] const char* semaphore_name)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        sem_unlink(semaphore_name);
        sem_close((sem_t*)semaphore);
        return 0;
     }
 #ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_close_sem((struct evl_sem*)semaphore);
     }
 #endif
}

template<ThreadType type>
inline int semaphore_wait(void* semaphore)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return sem_wait((sem_t*)semaphore);
     }
 #ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_get_sem((struct evl_sem*)semaphore);
     }
 #endif
}

template<ThreadType type>
inline int semaphore_signal(void* semaphore)
{
     if constexpr (type == ThreadType::PTHREAD)
     {
        return sem_post((sem_t*)semaphore);
     }
 #ifdef TWINE_BUILD_WITH_XENOMAI
     else if constexpr (type == ThreadType::XENOMAI)
     {
         return evl_put_sem((struct evl_sem*)semaphore);
     }
 #endif
}
} // namespace twine

#endif //TWINE_THREAD_HELPERS_H
