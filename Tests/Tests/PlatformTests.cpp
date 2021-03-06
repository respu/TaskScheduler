#include "Tests.h"
#include <UnitTest++.h>
#include <MTScheduler.h>

SUITE(PlatformTests)
{
	static const int DATA_VALUE = 13;

	uint32 g_Variable = 0;

	void MyThreadFunc(void* userData)
	{
		uint32 data = (uint32)userData;

		CHECK(data == DATA_VALUE);

		g_Variable = data;
	}

	TEST(ThreadTest)
	{
		uint32 data = DATA_VALUE;

		MT::Thread thread;
		thread.Start(32768, MyThreadFunc, (void*)data);
		thread.Stop();

		CHECK(g_Variable == DATA_VALUE);
	}
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	MT::Event g_Event;

	void MyThreadFunc2(void*)
	{
		MT::Thread::SpinSleep(300);
		g_Event.Signal();
	}

	TEST(EventTest)
	{
		g_Event.Create(MT::EventReset::MANUAL, false);

		MT::Thread thread;
		thread.Start(32768, MyThreadFunc2, nullptr);

		bool res0 = g_Event.Wait(100);
		bool res1 = g_Event.Wait(1000);

		CHECK(!res0);
		CHECK(res1);

		thread.Stop();
	}

	TEST(SleepTest)
	{

		MT::Timer timer;

		MT::Thread::SpinSleep(100);

		CHECK( timer.GetPastMilliSeconds() >= 100 );
	}
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
