#ifndef TWINE_TWINE_H_
#define TWINE_TWINE_H_

#include <memory>

namespace twine {

/**
 * @brief Function to determine the realtime processing state of the current thread
 * @return true if called from a realtime audio processing thread, false other
 */
bool is_current_thread_realtime();

typedef void (*WorkerCallback)(void* data);

class WorkerPool
{
public:
    /**
     * @brief Construct a WorkerPool object.
     * @param cores The maximum number of cores to use, must not be higher
     *              than the number of cores on the machine.
     * @return
     */
    static std::unique_ptr<WorkerPool> CreateWorkerPool(int cores);

    virtual ~WorkerPool() = default;

    virtual int add_worker(WorkerCallback worker_cb, void* worker_data) = 0;

    virtual void wait_for_workers_idle() = 0;

    virtual void wakeup_workers() = 0;

protected:
    WorkerPool() = default;
};


}// namespace twine

#endif // TWINE_TWINE_H_