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

#include <ucontext.h>
#include <stdlib.h>

namespace MT
{

	//
	//
	//
	class Fiber
	{
		void * funcData;
		TThreadEntryPoint func;

		ucontext_t fiberContext;
		bool isInitialized;

		static void FiberFuncInternal(void *pFiber)
		{
			Fiber* self = (Fiber*)pFiber;
			self->func(self->funcData);
		}

	private:

		Fiber(const Fiber &) {}
		void operator=(const Fiber &) {}

	public:

		Fiber()
			: isInitialized(false)
		{
		}

		~Fiber()
		{
			if (isInitialized)
			{
				if (func != nullptr)
				{
					free(fiberContext.uc_stack.ss_sp);
				}
				isInitialized = false;
			}
		}


		void CreateFromThread(Thread & thread)
		{
			ASSERT(!isInitialized, "Already initialized");
			ASSERT(thread.IsCurrentThread(), "Can't create fiber from this thread");

			ucontext_t m;
			fiberContext.uc_link = &m;

			int res = getcontext(&fiberContext);
			ASSERT(res == 0, "getcontext - failed");

			fiberContext.uc_link = nullptr;
			fiberContext.uc_stack.ss_sp = thread.GetStackBase();
			fiberContext.uc_stack.ss_size = thread.GetStackSize();
			fiberContext.uc_stack.ss_flags = 0;

			func = nullptr;
			funcData = nullptr;

			isInitialized = true;
		}


		void Create(size_t stackSize, TThreadEntryPoint entryPoint, void *userData)
		{
			ASSERT(!isInitialized, "Already initialized");

			func = entryPoint;
			funcData = userData;

			int res = getcontext(&fiberContext);
			ASSERT(res == 0, "getcontext - failed");

			fiberContext.uc_link = nullptr;
			fiberContext.uc_stack.ss_sp = malloc(stackSize);
			fiberContext.uc_stack.ss_size = stackSize;
			fiberContext.uc_stack.ss_flags = 0;
			makecontext(&fiberContext, (void(*)())&FiberFuncInternal, 1, this);

			isInitialized = true;
		}

		static void SwitchTo(Fiber & from, Fiber & to)
		{
			 __sync_synchronize();

			ASSERT(from.isInitialized, "Invalid from fiber");
			ASSERT(to.isInitialized, "Invalid to fiber");
			int res = swapcontext(&from.fiberContext, &to.fiberContext);
			ASSERT(res == 0, "setcontext - failed");
		}



	};


}


