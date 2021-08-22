#pragma once
#include <deque>
#include <functional>

namespace engine{

class DeletionQueue
{
private:
	std::deque<std::function<void()>> deletors;

public:
	void push(std::function<void()>&& function) 
	{
		deletors.push_back(function);
	}

	void flush() 
	{
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) 
		{
			(*it)();
		}
		deletors.clear();
	}
};

}