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

// �߳�����
uint32_t num_threads = 0;  

// �̳߳ش�С
constexpr size_t job_pool_capacity = 256;

// ����ء�
RingBuffer<std::function<void()>,job_pool_capacity> job_pool;  

// �̻߳��ѡ�
std::condition_variable wake_condition;   
std::mutex wake_mutex;    

// ��־λ��
uint64_t current_bit = 0; 
std::atomic<uint64_t> finished_bit;

// �����߳�
inline void poll()
{
	// ����һ���߳�
	wake_condition.notify_one();

	// �����߳����µ���
	std::this_thread::yield(); 
}

void initialize()
{
	finished_bit.store(0);

	// ����CPU�߳�����
	auto num_cores = std::thread::hardware_concurrency();
	num_threads = std::max(1u,num_cores);

	// ��ʼʱ�ʹ���ȫ���Ĺ����̡߳�
	for(uint32_t thread_id = 0; thread_id<num_threads; thread_id++)
	{
		std::thread worker([]
		{
			// ��ʼ��ʱʹ�ÿ��������������
			std::function<void()> job;
			while(true)
			{
				// ������һ������
				if(job_pool.pop_front(job))
				{
					// ������ִ���������ִ������
					job();

					// ���±�־λ��
					finished_bit.fetch_add(1);
				}
				else
				{
					// ���Ṥ��ֱ���̱߳����ѡ�
					std::unique_lock<std::mutex> lock(wake_mutex);
					wake_condition.wait(lock);
				}
			}
		});

		// �ͷſ���Ȩ
		worker.detach();
	}
}

void execute(const std::function<void()>& job)
{
	// ���±�־λ
	current_bit += 1;

	// ѭ��ѹ��һ������ֱ���ɹ���
	while(!job_pool.push_back(job))
	{
		// ��������������һ���߳�
		poll();
	}

	// ����һ���̡߳�
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

	// �����߳�����
	const uint32_t group_count = (job_count + group_size - 1) / group_size;

	// ���±�־λ
	current_bit += group_count;

	for(uint32_t group_index = 0; group_index < group_count; ++group_index)
	{
		// Ϊÿ���߳������ɶ�Ӧ������
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

		// ѭ��ѹ��һ������ֱ���ɹ���
		while(!job_pool.push_back(job_group))
		{
			poll();
		}

		// ����һ���߳�
		wake_condition.notify_one();
	}
}

}}