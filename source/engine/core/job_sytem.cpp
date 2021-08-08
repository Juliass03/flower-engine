/**
* NOTE: 简单但高效的JobSytem实现
*       大部分参考如下博文：
*       https://blog.molecular-matters.com/2015/08/24/job-system-2-0-lock-free-work-stealing-part-1-basics/
*       https://blog.molecular-matters.com/2015/09/08/job-system-2-0-lock-free-work-stealing-part-2-a-specialized-allocator/
*       https://blog.molecular-matters.com/2015/09/25/job-system-2-0-lock-free-work-stealing-part-3-going-lock-free/
*       https://blog.molecular-matters.com/2015/11/09/job-system-2-0-lock-free-work-stealing-part-4-parallel_for/
**/

#include "job_system.h"
#include <stdint.h>
#include <atomic>

namespace engine{

template<typename Worker,size_t COUNT>
class WorkStealingDequeue
{
	// 类型Worker将会频繁发生值复制因此必须保证为POD类型
	static_assert(std::is_pod<Worker>::value, "Worker must be pod type because we will copy values everywhere.");
	static_assert(!(COUNT & (COUNT - 1)), "COUNT must be a power of 2.");
	static constexpr size_t MASK = COUNT - 1;

	std::atomic<int64_t> m_top {0};
	std::atomic<int64_t> m_bottom {0};

	Worker m_workers[COUNT];

	// 返回值而不是引用来避免其他线程调用std::move方法引起的条件竞争
	Worker at(int64_t i) noexcept
	{
		return m_workers[i & MASK];
	}

	void set(int64_t i,Worker e) noexcept
	{
		m_workers[i & MASK] = e;
	}

public:
	// 队列的最大数目
	size_t getSize() const noexcept
	{
		return COUNT;
	}

	// 返回worker的数目
	size_t getCount() const noexcept
	{
		int64_t bottom = m_bottom.load(std::memory_order_relaxed);
		int64_t top = m_top.load(std::memory_order_relaxed);

		return bottom - top;
	}

	// 填入一个worker到队列底部
	// NOTE: 仅在主线程中调用
	void push(Worker worker) noexcept
	{
		int64_t bottom = m_bottom.load(std::memory_order_relaxed);
		set(bottom,worker);

		m_bottom.store(bottom + 1,std::memory_order_release);
	}

	// 从队列底部弹出一个worker
	// NOTE: 仅在主线程中调用
	Worker pop() noexcept
	{
		// fetch_sub原子的减一然后保存最后返回原先存储的值
		int64_t bottom = m_bottom.fetch_sub(1, std::memory_order_seq_cst) - 1;

		// 队列为空时则为-1
		assert(bottom >= -1);

		// memory_order_seq_cst确保顺序一致性
		int64_t top = m_top.load(std::memory_order_seq_cst);

		// 队列非空且未到最后一项
		if(top < bottom) 
		{
			// 直接返回
			return at(bottom);
		}

		// Worker为POD类型
		Worker item { };
		if(top == bottom) // 队列到了最后一项
		{
			// 无论如何先拿到最后一项的任务
			item = at(bottom);

			// Api: compare_exchange_strong(want_value,new_value)
			// 传入一个希望的值和一个新的值，若变量的值与期待的值(want_value)一致则替换为指定的新的值(new_value)，否则将变量的值与期待的值(want_value)交换

			/**
			 * NOTE: 现在已经到了队列的最后一项，此处可能会和steal()发生条件竞争
			 *       因此，使用compare_exchange_strong来判断:
			 *       1. 执行到此处时的top值与m_top仍然相同，证明队列最后一项任务没有被窃取。
			 *       2. 执行到此处时的top值与m_top不相同，队列最后一项被窃取了。
			**/
			if(m_top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
			{
				// 任务没有被窃取因此可以直接返回
				// compare_exchange_strong执行结果为 m_top = top + 1
				top++; // 这时需要更新top的值
			}
			else
			{
				// 任务被窃取了因此此时队列为空
				// compare_exchange_strong执行结果为 m_top = top
				item = Worker(); // 返回的任务应为空
			}
		}
		else
		{
			// 在m_top.load(std::memory_order_seq_cst)操作前若发生了窃取操作
			// top的值会 +1 此时会进入这里，这里什么都不做，在最后将top的值存到m_buttom中
			assert(top - bottom == 1);
		}

		m_bottom.store(top, std::memory_order_relaxed);
		return item;
	}

	// 从队列顶部窃取worker
	// NOTE: 线程安全
	Worker steal() noexcept
	{
		while (true) 
		{
			int64_t top    = m_top.load(   std::memory_order_seq_cst);
			int64_t bottom = m_bottom.load(std::memory_order_seq_cst);

			// 空白队列
			if (top >= bottom) 
			{
				return Worker();
			}

			// 队列非空情况
			Worker item(at(top));

			// 使用compare_exchange_strong确保窃取过程中队列尾部的任务没有被弹出
			if (m_top.compare_exchange_strong(top, top + 1,std::memory_order_seq_cst,std::memory_order_relaxed)) 
			{
				// 窃取成功直接返回
				// 此时m_top = top +1
				return item;
			}

			// 窃取失败再来一次
		}
	}
};




}