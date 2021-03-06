/**
 ******************************************************************************
 * @file    spark_wiring_thread.h
 * @authors Matthew McGowan
 * @date    03 March 2015
 ******************************************************************************
  Copyright (c) 2015 Particle Industries, Inc.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

#ifndef SPARK_WIRING_THREAD_H
#define SPARK_WIRING_THREAD_H

#if PLATFORM_THREADING

#include "concurrent_hal.h"
#include <stddef.h>
#include <mutex>
#include <functional>
#include <type_traits>

typedef std::function<os_thread_return_t(void)> wiring_thread_fn_t;

class SingleThreadedSection {
public:
    SingleThreadedSection() {
        os_thread_scheduling(false, NULL);
    }

    ~SingleThreadedSection() {
        os_thread_scheduling(true, NULL);
    }
};

#define SINGLE_THREADED_SECTION()  SingleThreadedSection __cs;

#define SINGLE_THREADED_BLOCK() for (bool __todo = true; __todo; ) for (SingleThreadedSection __cs; __todo; __todo=0)
#define WITH_LOCK(lock) for (bool __todo = true; __todo;) for (std::lock_guard<decltype(lock)> __lock((lock)); __todo; __todo=0)
#define TRY_LOCK(lock) for (bool __todo = true; __todo; ) for (std::unique_lock<typename std::remove_reference<decltype(lock)>::type> __lock##lock((lock), std::try_to_lock); __todo &= bool(__lock##lock); __todo=0)

#else
#define SINGLE_THREADED_SECTION()
#define SINGLE_THREADED_BLOCK()
#define WITH_LOCK(x)
#define TRY_LOCK(x)
#endif

#if PLATFORM_THREADING

class Thread
{
private:
    mutable os_thread_t handle = OS_THREAD_INVALID_HANDLE;
    mutable wiring_thread_fn_t *wrapper = NULL;
    os_thread_fn_t func_ = NULL;
    void* func_param_ = NULL;
    bool exited_ = false;

public:
    Thread() : handle(OS_THREAD_INVALID_HANDLE) {}

    Thread(const char* name, os_thread_fn_t function, void* function_param=NULL,
            os_thread_prio_t priority=OS_THREAD_PRIORITY_DEFAULT, size_t stack_size=OS_THREAD_STACK_SIZE_DEFAULT)
        : wrapper(NULL),
          func_(function),
          func_param_(function_param)
    {
        os_thread_create(&handle, name, priority, &Thread::run, this, stack_size);
    }

    Thread(const char *name, wiring_thread_fn_t function,
            os_thread_prio_t priority=OS_THREAD_PRIORITY_DEFAULT, size_t stack_size=OS_THREAD_STACK_SIZE_DEFAULT)
        : handle(OS_THREAD_INVALID_HANDLE), wrapper(NULL)
    {
        if(function) {
            wrapper = new wiring_thread_fn_t(function);
            os_thread_create(&handle, name, priority, &Thread::run, this, stack_size);
        }
    }

    ~Thread()
    {
        dispose();
    }

    void dispose()
    {
        if (!is_valid())
            return;

        // We shouldn't dispose of current thread
        if (is_current())
            return;

        if (!exited_) {
            join();
        }

        if (wrapper) {
            delete wrapper;
            wrapper = NULL;
        }

        os_thread_cleanup(handle);
        handle = OS_THREAD_INVALID_HANDLE;
    }

    bool join()
    {
        return is_valid() && os_thread_join(handle)==0;
    }

    bool cancel()
    {
        return is_valid() && os_thread_exit(handle)==0;
    }

    bool is_valid()
    {
        // TODO should this also check xTaskIsTaskFinished as well?
        return handle!=OS_THREAD_INVALID_HANDLE;
    }

    bool is_current()
    {
        return os_thread_is_current(handle);
    }

    Thread& operator = (const Thread& rhs)
    {
        if (this != &rhs)
        {
            this->handle = rhs.handle;
            this->wrapper = rhs.wrapper;
            this->func_ = rhs.func_;
            this->func_param_ = rhs.func_param_;
            this->exited_ = rhs.exited_;
            rhs.handle = OS_THREAD_INVALID_HANDLE;
            rhs.wrapper = NULL;
        }
        return *this;
    }

private:

    static os_thread_return_t run(void* param) {
        Thread* th = (Thread*)param;
        if (th->func_) {
            (*(th->func_))(th->func_param_);
        } else if (th->wrapper) {
            (*(th->wrapper))();
        }
        th->exited_ = true;
        os_thread_exit(nullptr);
    }
};

class Mutex
{
    os_mutex_t handle_;
public:
    /**
     * Creates a shared mutex from an existing handle.
     * This is mainly used to share mutexes between dynamically linked modules.
     */
    Mutex(os_mutex_t handle) : handle_(handle) {}

    /**
     * Creates a new mutex.
     */
    Mutex() : handle_(nullptr)
    {
        os_mutex_create(&handle_);
    }

    void dispose()
    {
        if (handle_) {
            os_mutex_destroy(handle_);
            handle_ = nullptr;
        }
    }

    void lock() { os_mutex_lock(handle_); }
    bool trylock() { return os_mutex_trylock(handle_)==0; }
    void unlock() { os_mutex_unlock(handle_); }

};


class RecursiveMutex
{
    os_mutex_recursive_t handle_;
public:
    /**
     * Creates a shared mutex.
     */
    RecursiveMutex(os_mutex_recursive_t handle) : handle_(handle) {}

    RecursiveMutex() : handle_(nullptr)
    {
        os_mutex_recursive_create(&handle_);
    }

    void dispose()
    {
        if (handle_) {
            os_mutex_recursive_destroy(handle_);
            handle_ = nullptr;
        }
    }

    void lock() { os_mutex_recursive_lock(handle_); }
    bool trylock() { return os_mutex_recursive_trylock(handle_)==0; }
    void unlock() { os_mutex_recursive_unlock(handle_); }

};

#endif // PLATFORM_THREADING

namespace particle {

// Class implementing a dummy concurrency policy
class NoConcurrency {
public:
    struct Lock {
    };

    Lock lock() const {
        return Lock();
    }

    void unlock(Lock) const {
    }
};

} // namespace particle

#endif  /* SPARK_WIRING_THREAD_H */

