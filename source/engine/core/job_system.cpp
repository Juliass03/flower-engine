#include <algorithm>  
#include <atomic>   
#include <thread> 
#include <condition_variable> 
#include <stdint.h>
#include <mutex>
#include "job_system.h"

namespace engine{ namespace jobsystem{

template <typename T, size_t capacity>
class RingBuffer
{
public:
	RingBuffer() = default;
	~RingBuffer() = default;

	inline bool push_back(const T& item)
	{
		bool result = false;
		lock.lock();
		size_t next = (head+1)%capacity;
		if(next!=tail)
		{
			data[head] = item;
			head = next;
			result = true;
		}
		lock.unlock();
		return result;
	}

	inline bool pop_front(T& item)
	{
		bool result = false;
		lock.lock();
		if(tail!=head)
		{
			item = data[tail];
			tail = (tail+1)%capacity;
			result = true;
		}
		lock.unlock();
		return result;
	}
private:
	T data[capacity];
	size_t head = 0;
	size_t tail = 0;
	std::mutex lock;
};

// 线程数。
uint32_t num_threads = 0;  

// 线程池大小
constexpr size_t job_pool_capacity = 256;

// 任务池。
RingBuffer<std::function<void()>,job_pool_capacity> job_pool;  

// 线程唤醒。
std::condition_variable wake_condition;   
std::mutex wake_mutex;    

// 标志位。
uint64_t current_bit = 0; 
std::atomic<uint64_t> finished_bit;

// 弹出线程
inline void poll()
{
	// 唤醒一个线程
	wake_condition.notify_one();

	// 允许线程重新调度
	std::this_thread::yield(); 
}

void initialize()
{
	finished_bit.store(0);

	// 计算CPU线程数。
	auto num_cores = std::thread::hardware_concurrency();
	num_threads = std::max(1u,num_cores);

	// 开始时就创建全部的工作线程。
	for(uint32_t thread_id = 0; thread_id<num_threads; thread_id++)
	{
		std::thread worker([]
		{
			// 初始化时使用空任务填满任务池
			std::function<void()> job;
			while(true)
			{
				// 弹出第一个任务。
				if(job_pool.pop_front(job))
				{
					// 如果发现存在任务则执行它。
					job();

					// 更新标志位。
					finished_bit.fetch_add(1);
				}
				else
				{
					// 不会工作直到线程被唤醒。
					std::unique_lock<std::mutex> lock(wake_mutex);
					wake_condition.wait(lock);
				}
			}
		});

		// 释放控制权
		worker.detach();
	}
}

void execute(const std::function<void()>& job)
{
	// 更新标志位
	current_bit += 1;

	// 循环压入一个任务直到成功。
	while(!job_pool.push_back(job))
	{
		// 若队列已满则唤醒一个线程
		poll();
	}

	// 唤醒一个线程。
	wake_condition.notify_one(); 
}

bool isBusy()
{
	return finished_bit.load() < current_bit;
}

void waitForAll()
{
	while(isBusy())
	{
		poll();
	}
}

void destroy()
{
	waitForAll();
}

void dispatch(const uint32_t& job_count,const uint32_t& group_size,const std::function<void(DispatchArgs)>& job)
{
	if( job_count==0 || group_size ==0)
	{
		return;
	}

	// 计算线程组数
	const uint32_t group_count = (job_count + group_size - 1) / group_size;

	// 更新标志位
	current_bit += group_count;

	for(uint32_t group_index = 0; group_index < group_count; ++group_index)
	{
		// 为每个线程组生成对应的任务
		auto job_group = [job_count,group_size,job,group_index]()
		{
			const uint32_t group_job_fffset = group_index * group_size;
			const uint32_t group_job_end = std::min(group_job_fffset + group_size,job_count);

			DispatchArgs args;
			args.groupId = group_index;

			for(uint32_t i = group_job_fffset; i<group_job_end; ++i)
			{
				args.jobId = i;
				job(args);
			}
		};

		// 循环压入一个任务直到成功。
		while(!job_pool.push_back(job_group))
		{
			poll();
		}

		// 唤醒一个线程
		wake_condition.notify_one();
	}
}

}}