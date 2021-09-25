#pragma once
#include <chrono>
#include "runtime_module.h"

namespace engine{

class Timer
{
    friend class EngineLoop;
private:
    // 初始化的TimePoint,用于计算真实世界的时间流逝。
    std::chrono::system_clock::time_point mInitTimePoint{ };
    std::chrono::duration<float> mDeltaTime { 0.0f };

    // 游戏中的TimePoint,用于计算游戏中的时间流逝。
    std::chrono::system_clock::time_point mGameTimePoint{ };
    std::chrono::duration<float> mGameDeltaTime { 0.0f };

    // 游戏中的FramePoint,用于计算游戏中的DeltaTime。
    std::chrono::system_clock::time_point mFrameTimePoint{ };
    std::chrono::duration<float> mFrameDeltaTime { 0.0f };

    bool bGamePause = true;
    float m_currentFps {0.0f};
    float m_currentSmoothFps {0.0f};

    uint64 m_frameCount = 0;

public:
    float getCurrentFps() const { return m_currentFps;}
    float getCurrentSmoothFps() const { return m_currentSmoothFps;}

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

    auto getFrameCount() const
    {
        return m_frameCount;
    }

    void frameCountAdd()
    {
        m_frameCount++;
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