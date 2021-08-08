/**
* NOTE: �򵥵���Ч��JobSytemʵ��
*       �󲿷ֲο����²��ģ�
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
	// ����Worker����Ƶ������ֵ������˱��뱣֤ΪPOD����
	static_assert(std::is_pod<Worker>::value, "Worker must be pod type because we will copy values everywhere.");
	static_assert(!(COUNT & (COUNT - 1)), "COUNT must be a power of 2.");
	static constexpr size_t MASK = COUNT - 1;

	std::atomic<int64_t> m_top {0};
	std::atomic<int64_t> m_bottom {0};

	Worker m_workers[COUNT];

	// ����ֵ���������������������̵߳���std::move�����������������
	Worker at(int64_t i) noexcept
	{
		return m_workers[i & MASK];
	}

	void set(int64_t i,Worker e) noexcept
	{
		m_workers[i & MASK] = e;
	}

public:
	// ���е������Ŀ
	size_t getSize() const noexcept
	{
		return COUNT;
	}

	// ����worker����Ŀ
	size_t getCount() const noexcept
	{
		int64_t bottom = m_bottom.load(std::memory_order_relaxed);
		int64_t top = m_top.load(std::memory_order_relaxed);

		return bottom - top;
	}

	// ����һ��worker�����еײ�
	// NOTE: �������߳��е���
	void push(Worker worker) noexcept
	{
		int64_t bottom = m_bottom.load(std::memory_order_relaxed);
		set(bottom,worker);

		m_bottom.store(bottom + 1,std::memory_order_release);
	}

	// �Ӷ��еײ�����һ��worker
	// NOTE: �������߳��е���
	Worker pop() noexcept
	{
		// fetch_subԭ�ӵļ�һȻ�󱣴���󷵻�ԭ�ȴ洢��ֵ
		int64_t bottom = m_bottom.fetch_sub(1, std::memory_order_seq_cst) - 1;

		// ����Ϊ��ʱ��Ϊ-1
		assert(bottom >= -1);

		// memory_order_seq_cstȷ��˳��һ����
		int64_t top = m_top.load(std::memory_order_seq_cst);

		// ���зǿ���δ�����һ��
		if(top < bottom) 
		{
			// ֱ�ӷ���
			return at(bottom);
		}

		// WorkerΪPOD����
		Worker item { };
		if(top == bottom) // ���е������һ��
		{
			// ����������õ����һ�������
			item = at(bottom);

			// Api: compare_exchange_strong(want_value,new_value)
			// ����һ��ϣ����ֵ��һ���µ�ֵ����������ֵ���ڴ���ֵ(want_value)һ�����滻Ϊָ�����µ�ֵ(new_value)�����򽫱�����ֵ���ڴ���ֵ(want_value)����

			/**
			 * NOTE: �����Ѿ����˶��е����һ��˴����ܻ��steal()������������
			 *       ��ˣ�ʹ��compare_exchange_strong���ж�:
			 *       1. ִ�е��˴�ʱ��topֵ��m_top��Ȼ��ͬ��֤���������һ������û�б���ȡ��
			 *       2. ִ�е��˴�ʱ��topֵ��m_top����ͬ���������һ���ȡ�ˡ�
			**/
			if(m_top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
			{
				// ����û�б���ȡ��˿���ֱ�ӷ���
				// compare_exchange_strongִ�н��Ϊ m_top = top + 1
				top++; // ��ʱ��Ҫ����top��ֵ
			}
			else
			{
				// ������ȡ����˴�ʱ����Ϊ��
				// compare_exchange_strongִ�н��Ϊ m_top = top
				item = Worker(); // ���ص�����ӦΪ��
			}
		}
		else
		{
			// ��m_top.load(std::memory_order_seq_cst)����ǰ����������ȡ����
			// top��ֵ�� +1 ��ʱ������������ʲô�������������top��ֵ�浽m_buttom��
			assert(top - bottom == 1);
		}

		m_bottom.store(top, std::memory_order_relaxed);
		return item;
	}

	// �Ӷ��ж�����ȡworker
	// NOTE: �̰߳�ȫ
	Worker steal() noexcept
	{
		while (true) 
		{
			int64_t top    = m_top.load(   std::memory_order_seq_cst);
			int64_t bottom = m_bottom.load(std::memory_order_seq_cst);

			// �հ׶���
			if (top >= bottom) 
			{
				return Worker();
			}

			// ���зǿ����
			Worker item(at(top));

			// ʹ��compare_exchange_strongȷ����ȡ�����ж���β��������û�б�����
			if (m_top.compare_exchange_strong(top, top + 1,std::memory_order_seq_cst,std::memory_order_relaxed)) 
			{
				// ��ȡ�ɹ�ֱ�ӷ���
				// ��ʱm_top = top +1
				return item;
			}

			// ��ȡʧ������һ��
		}
	}
};




}