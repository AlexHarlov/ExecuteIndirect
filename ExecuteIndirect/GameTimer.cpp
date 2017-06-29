//***************************************************************************************
// GameTimer.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include <windows.h>
#include "GameTimer.h"

GameTimer::GameTimer()
: mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0), 
  mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSecondsPerCount = 1.0 / (double)countsPerSec;
}

/// <summary>
// Returns the total time elapsed since Reset() was called, NOT counting any
// time when the clock is stopped.
/// </summary>
/// <returns></returns>
float GameTimer::TotalTime()const
{
	// If we are stopped, do not count the time that has passed since we stopped.
	// Moreover, if we previously already had a pause, the distance 
	// mStopTime - mBaseTime includes paused time, which we do not want to count.
	// To correct this, we can subtract the paused time from mStopTime:  
	if( mStopped )
	{
		return (float)(((mStopTime - mPausedTime)-mBaseTime)*mSecondsPerCount);
	}

	// The distance mCurrTime - mBaseTime includes paused time,
	// which we do not want to count.  To correct this, we can subtract 
	// the paused time from mCurrTime:  
	else
	{
		return (float)(((mCurrTime-mPausedTime)-mBaseTime)*mSecondsPerCount);
	}
}

/// <summary>
/// Get the delta time
/// </summary>
/// <returns>delta time</returns>
float GameTimer::DeltaTime()const
{
	return (float)mDeltaTime;
}

/// <summary>
/// Resets the time, initializing base time with the current value of the performance counter
/// </summary>
void GameTimer::Reset()
{
	__int64 currTime;
	//Retrieves the current value of the performance counter, which is a high resolution (<1us) 
	//time stamp that can be used for time-interval measurements.
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	//Initialize base and previous time with the current time
	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped  = false;
}

/// <summary>
/// Starts the timer.
/// </summary>
void GameTimer::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);


	// Accumulate the time elapsed between stop and start pairs.
	if( mStopped )
	{
		mPausedTime += (startTime - mStopTime);	

		mPrevTime = startTime;
		mStopTime = 0;
		mStopped  = false;
	}
}

void GameTimer::Stop()
{
	if( !mStopped )
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mStopTime = currTime;
		mStopped  = true;
	}
}

void GameTimer::Tick()
{
	if( mStopped )
	{
		mDeltaTime = 0.0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	// Time difference between this frame and the previous.
	mDeltaTime = (mCurrTime - mPrevTime)*mSecondsPerCount;

	// Prepare for next frame.
	mPrevTime = mCurrTime;

	// Guard against negative values
	if(mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}

