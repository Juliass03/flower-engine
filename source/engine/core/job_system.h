#pragma once
#include <stdint.h>
#include <functional>

namespace engine{ namespace jobsystem {

// 初始化。
void initialize();

// 添加任务以异步执行。
void execute(const std::function<void()>& job);

// 线程分发参数
struct DispatchArgs
{
	uint32_t jobId;
	uint32_t groupId;
};

// 将一个任务分发到多个子任务中并行执行。
void dispatch(const uint32_t& jobCount,const uint32_t& groupSize,const std::function<void(DispatchArgs)>& job);

// 检测是否有任务正在工作。
bool isBusy();

// 等待所有任务执行完毕。
void waitForAll();

// 结束所有的Jop。
void destroy();

}}