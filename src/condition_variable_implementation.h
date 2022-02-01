/*
 * Copyright 2018-2021 Modern Ancient Instruments Networked AB, dba Elk
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

#ifndef TWINE_CONDITION_VARIABLE_IMPLEMENTATION_H
#define TWINE_CONDITION_VARIABLE_IMPLEMENTATION_H

#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <cstring>
#include <cassert>

#include "twine_internal.h"

#ifdef TWINE_BUILD_WITH_XENOMAI
#include <poll.h>
#include <sys/eventfd.h>
#include <evl/mutex.h>
#include <evl/event.h>
// #include <rtdm/ipc.h>
// #include <cobalt/sys/socket.h>
#endif

namespace twine {

/**
 * @brief Implementation with regular c++ std library constructs for
 *        use in a regular linux context.
 */
class PosixConditionVariable : public RtConditionVariable
{
public:
    ~PosixConditionVariable() override = default;

    void notify() override;

    bool wait() override;

private:
    bool                    _flag{false};
    std::mutex              _mutex;
    std::condition_variable _cond_var;
};

void PosixConditionVariable::notify()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _flag = true;
    _cond_var.notify_one();
}

bool PosixConditionVariable::wait()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _cond_var.wait(lock);
    bool notified = _flag;
    _flag = false;
    return notified;
}

#ifdef TWINE_BUILD_WITH_XENOMAI


/**
 * @brief Implementation using xenomai xddp queues that allow signalling a
 *        non xenomai thread from a xenomai thread.
 */
class XenomaiConditionVariable : public RtConditionVariable
{
public:
    XenomaiConditionVariable() = default;

    ~XenomaiConditionVariable() override = default;

    void notify() override;

    bool wait() override;

private:
    struct evl_mutex lock = EVL_MUTEX_INITIALIZER("this_lock",
       		 EVL_CLOCK_MONOTONIC, 0, EVL_MUTEX_NORMAL|EVL_CLONE_PRIVATE);
    struct evl_event event = EVL_EVENT_INITIALIZER("this_event",
       		 EVL_CLOCK_MONOTONIC, EVL_CLONE_PRIVATE);
    bool condition = false;

};


void XenomaiConditionVariable::notify()
{

	evl_lock_mutex(&lock);
	condition = true;
	evl_signal_event(&event);
	evl_unlock_mutex(&lock);

}

bool XenomaiConditionVariable::wait()
{
	evl_lock_mutex(&lock);
	/*
	 * Check whether the wait condition is satisfied, go
	 * sleeping if not. EVL drops the lock and puts the thread
	 * to sleep atomically, then re-acquires it automatically
	 * once the event is signaled before returning to the
	 * caller.
	 */
	while (!condition)
	      evl_wait_event(&event, &lock);

	evl_unlock_mutex(&lock);
    return condition;
}

#endif
}// namespace twine

#endif //TWINE_CONDITION_VARIABLE_IMPLEMENTATION_H
