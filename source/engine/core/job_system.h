/**
 * NOTE: 简单但高效的JobSytem实现
 *       大部分参考如下博文：
 *       https://blog.molecular-matters.com/2015/08/24/job-system-2-0-lock-free-work-stealing-part-1-basics/
 *       https://blog.molecular-matters.com/2015/09/08/job-system-2-0-lock-free-work-stealing-part-2-a-specialized-allocator/
 *       https://blog.molecular-matters.com/2015/09/25/job-system-2-0-lock-free-work-stealing-part-3-going-lock-free/
 *       https://blog.molecular-matters.com/2015/11/09/job-system-2-0-lock-free-work-stealing-part-4-parallel_for/
**/
#pragma once
#include "core.h"
#include <functional>
#include <atomic>

namespace engine{ namespace job_system{ 


}}