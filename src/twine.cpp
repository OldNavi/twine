#include "twine.h"
#include "twine_internal.h"
#include "xenomai_worker_pool.h"
#include "std_worker_pool.h"

namespace twine {

thread_local int ThreadRtFlag::_instance_counter = 0;
bool XenomaiRtFlag::_enabled = false;
static XenomaiRtFlag running_xenomai_realtime;

bool is_current_thread_realtime()
{
    return ThreadRtFlag::is_realtime();
}

void init_xenomai()
{
    running_xenomai_realtime.set(true);
}

std::unique_ptr<WorkerPool> WorkerPool::CreateWorkerPool()
{
    if (running_xenomai_realtime.is_set())
    {
        return std::make_unique<XenomaiWorkerPool>();
    }
    return std::make_unique<StdWorkerPool>();
}

} // twine