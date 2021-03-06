// The MIT License (MIT)
// 
// 	Copyright (c) 2015 Sergey Makeev, Vadim Slyusarev
// 
// 	Permission is hereby granted, free of charge, to any person obtaining a copy
// 	of this software and associated documentation files (the "Software"), to deal
// 	in the Software without restriction, including without limitation the rights
// 	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// 	copies of the Software, and to permit persons to whom the Software is
// 	furnished to do so, subject to the following conditions:
// 
//  The above copyright notice and this permission notice shall be included in
// 	all copies or substantial portions of the Software.
// 
// 	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// 	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// 	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// 	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// 	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// 	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// 	THE SOFTWARE.

#pragma once

#include <MTTools.h>
#include <MTPlatform.h>
#include <MTConcurrentQueueLIFO.h>
#include <MTStackArray.h>
#include <MTWrapperArray.h>
#include <MTThreadContext.h>
#include <MTFiberContext.h>
#include <MTTaskBase.h>

namespace MT
{
	const uint32 MT_MAX_THREAD_COUNT = 32;
	const uint32 MT_MAX_FIBERS_COUNT = 128;
	const uint32 MT_SCHEDULER_STACK_SIZE = 131072;
	const uint32 MT_FIBER_STACK_SIZE = 32768;

	namespace internal
	{
		struct ThreadContext;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Task scheduler
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	class TaskScheduler
	{
		friend class FiberContext;
		friend struct internal::ThreadContext;

		struct GroupStats
		{
			AtomicInt inProgressTaskCount;
			Event allDoneEvent;

			GroupStats()
			{
				inProgressTaskCount.Set(0);
				allDoneEvent.Create( EventReset::MANUAL, true );
			}
		};

		// Thread index for new task
		AtomicInt roundRobinThreadIndex;

		// Threads created by task manager
		uint32 threadsCount;
		internal::ThreadContext threadContext[MT_MAX_THREAD_COUNT];

		// Per group task statistic
		GroupStats groupStats[TaskGroup::COUNT];

		// All groups task statistic
		GroupStats allGroupStats;


		//Task awaiting group through FiberContext::WaitGroupAndYield call
		ConcurrentQueueLIFO<FiberContext*> waitTaskQueues[TaskGroup::COUNT];


		// Fibers pool
		ConcurrentQueueLIFO<FiberContext*> availableFibers;

		// Fibers context
		FiberContext fiberContext[MT_MAX_FIBERS_COUNT];

		FiberContext* RequestFiberContext(internal::GroupedTask& task);
		void ReleaseFiberContext(FiberContext* fiberExecutionContext);
		void RunTasksImpl(WrapperArray<internal::TaskBucket>& buckets, FiberContext * parentFiber, bool restoredFromAwaitState);

		static void ThreadMain( void* userData );
		static void FiberMain( void* userData );
		static bool StealTask(internal::ThreadContext& threadContext, internal::GroupedTask & task);

		static FiberContext* ExecuteTask (internal::ThreadContext& threadContext, FiberContext* fiberContext);

	public:

		TaskScheduler();
		~TaskScheduler();

		template<class TTask>
		void RunAsync(TaskGroup::Type group, TTask* taskArray, uint32 taskCount);

		bool WaitGroup(TaskGroup::Type group, uint32 milliseconds);
		bool WaitAll(uint32 milliseconds);

		bool IsEmpty();

		uint32 GetWorkerCount() const;

		bool IsWorkerThread() const;

#ifdef MT_INSTRUMENTED_BUILD

		size_t GetProfilerEvents(uint32 workerIndex, MT::ProfileEventDesc * dstBuffer, size_t dstBufferSize);

#endif
	};
}

#include "MTScheduler.inl"
#include "MTFiberContext.inl"
