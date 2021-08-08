#pragma once
#include <chrono>
#include "runtime_module.h"

namespace engine{

class Timer
{
private:
    // ��ʼ����TimePoint,���ڼ�����ʵ�����ʱ�����š�
    std::chrono::system_clock::time_point mInitTimePoint{ };
    std::chrono::duration<float> mDeltaTime { 0.0f };

    // ��Ϸ�е�TimePoint,���ڼ�����Ϸ�е�ʱ�����š�
    std::chrono::system_clock::time_point mGameTimePoint{ };
    std::chrono::duration<float> mGameDeltaTime { 0.0f };

    // ��Ϸ�е�FramePoint,���ڼ�����Ϸ�е�DeltaTime��
    std::chrono::system_clock::time_point mFrameTimePoint{ };
    std::chrono::duration<float> mFrameDeltaTime { 0.0f };

    bool bGamePause = true;

public:
    Timer(){ }
    ~Timer() { }

    void globalTimeInit()
    {
        mInitTimePoint = std::chrono::system_clock::now();
    }

    float globalPassTime()
    {
        mDeltaTime = std::chrono::system_clock::now() - mInitTimePoint;
        return mDeltaTime.count();
    }

    bool isGamePause() const
    {
        return bGamePause;
    }

    void gameStart()
    {
        mGameTimePoint = std::chrono::system_clock::now();
        mGameDeltaTime = std::chrono::duration<float>(0.0f);
        bGamePause = false;
    }

    float gameTime()
    {
        if(bGamePause)
        {
            return mGameDeltaTime.count();
        }
        else
        {
            auto nowTime = std::chrono::system_clock::now();
            mGameDeltaTime += nowTime - mGameTimePoint;
            mGameTimePoint = nowTime;

            return mGameDeltaTime.count();
        }
    }

    void gamePause()
    {
        if(!bGamePause)
        {
            auto nowTime = std::chrono::system_clock::now();
            mGameDeltaTime += nowTime - mGameTimePoint;

            mGameTimePoint = nowTime;
            bGamePause = true;
        }
    }

    void gameContinue()
    {
        if(bGamePause)
        {
            mGameTimePoint = std::chrono::system_clock::now();
            bGamePause = false;
        }
    }

    void gameStop()
    {
        mGameTimePoint = std::chrono::system_clock::now();
        mGameDeltaTime = std::chrono::duration<float>(0.0f);

        bGamePause = true;
    }

    void frameTimeInit()
    {
        mFrameTimePoint = std::chrono::system_clock::now();
    }

    void frameTick()
    {
        mFrameDeltaTime = std::chrono::system_clock::now() - mFrameTimePoint;
    }

    float frameDeltaTime() const
    {
        return mFrameDeltaTime.count();
    }

    void frameReset()
    {
        mFrameTimePoint = std::chrono::system_clock::now();
    }
};

extern Timer g_timer;

}