#pragma once
#include <stdint.h>
#include <functional>

namespace engine{ namespace jobsystem {

// ��ʼ����
void initialize();

// ����������첽ִ�С�
void execute(const std::function<void()>& job);

// �̷ַ߳�����
struct DispatchArgs
{
	uint32_t jobId;
	uint32_t groupId;
};

// ��һ������ַ�������������в���ִ�С�
void dispatch(const uint32_t& jobCount,const uint32_t& groupSize,const std::function<void(DispatchArgs)>& job);

// ����Ƿ����������ڹ�����
bool isBusy();

// �ȴ���������ִ����ϡ�
void waitForAll();

// �������е�Jop��
void destroy();

}}