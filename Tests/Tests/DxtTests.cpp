#include "Tests.h"
#include <UnitTest++.h>
#include <MTScheduler.h>

#include <squish.h>
#include <string.h>

namespace EmbeddedImage
{
	#include "LenaDxt/LenaColor.h"
	#include "LenaDxt/LenaColorDXT1.h"
	#include "LenaDxt/HeaderDDS.h"
}


SUITE(DxtTests)
{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace DxtCompress
{

	struct SimpleRunParams : public MT::TaskBase<SimpleRunParams>
	{
		uint32 width;
		uint32 height;
		uint32 stride;
		uint8 * srcPixels;

		uint32 blkWidth;
		uint32 blkHeight;

		uint8 * dstBlocks;

		SimpleRunParams(uint32 _width, uint32 _height, uint32 _stride, uint8* _srcPixels)
		{
			width = _width;
			height = _height;
			stride = _stride;

			srcPixels = _srcPixels;

			blkWidth = width >> 2;
			blkHeight = height >> 2;
		}


		void Do(MT::FiberContext&)
		{
			for (uint32 blkY = 0; blkY < blkHeight; blkY++)
			{
				for (uint32 blkX = 0; blkX < blkWidth; blkX++)
				{
					// 16 pixels of input
					uint32 pixels[4*4];

					// copy dxt1 block from image
					for (int y = 0; y < 4; y++)
					{
						for (int x = 0; x < 4; x++)
						{
							int srcX = blkX * 4 + x;
							int srcY = blkY * 4 + y;

							int index = srcY * stride + (srcX * 3);

							uint8 r = srcPixels[index + 0];
							uint8 g = srcPixels[index + 1];
							uint8 b = srcPixels[index + 2];

							uint32 color = 0xFF000000 | ((b << 16) | (g << 8) | (r));

							pixels[y * 4 + x] = color;
						}
					}

					// 8 bytes of output
					int blockIndex = blkY * blkWidth + blkX;
					uint8 * dxtBlock = &dstBlocks[blockIndex*8];

					// compress the 4x4 block using DXT1 compression
					squish::Compress( (squish::u8 *)&pixels[0], dxtBlock, squish::kDxt1 );

				} // block iterator
			} // block iterator
		}
	};



	// dxt compressor simple test
	TEST(RunSimpleDxtCompress)
	{
		static_assert(ARRAY_SIZE(EmbeddedImage::lenaColor) == 49152, "Image size is invalid");
		static_assert(ARRAY_SIZE(EmbeddedImage::lenaColorDXT1) == 8192, "Image size is invalid");

		SimpleRunParams simpleTask(128, 128, 384, (uint8 *)&EmbeddedImage::lenaColor[0]);
		ASSERT ((simpleTask.width & 3) == 0 && (simpleTask.height & 3) == 0, "Image size must be a multiple of 4");

		int dxtBlocksTotalSize = simpleTask.blkWidth * simpleTask.blkHeight * 8;
		simpleTask.dstBlocks = (uint8 *)malloc( dxtBlocksTotalSize );
		memset(simpleTask.dstBlocks, 0x0, dxtBlocksTotalSize);

		MT::TaskScheduler scheduler;
		scheduler.RunAsync(MT::TaskGroup::GROUP_0, &simpleTask, 1);

		CHECK(scheduler.WaitAll(30000));
		CHECK_ARRAY_EQUAL(simpleTask.dstBlocks, EmbeddedImage::lenaColorDXT1, dxtBlocksTotalSize);

/*
		FILE * file = fopen("lena_dxt1.dds", "w+b");
		fwrite(&EmbeddedImage::ddsHeader[0], ARRAY_SIZE(EmbeddedImage::ddsHeader), 1, file);
		fwrite(taskParams.dstBlocks, dxtBlocksTotalSize, 1, file);
		fclose(file);
*/

		free(simpleTask.dstBlocks);
	}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	struct ComplexRunBlockSubtask : public MT::TaskBase<ComplexRunBlockSubtask>
	{
		int srcX;
		int srcY;

		int stride;

		uint8 * srcPixels;
		uint8 * dstBlock;

		ComplexRunBlockSubtask(int _srcX, int _srcY, int _stride, uint8* _srcPixels, uint8* _dstBlock )
		{
				srcX = _srcX;
				srcY = _srcY;
				stride = _stride;
				srcPixels = _srcPixels;
				dstBlock = _dstBlock;
		}

		void Do(MT::FiberContext&)
		{
			// 16 pixels of input
			uint32 pixels[4*4];

			// copy dxt1 block from image
			for (int y = 0; y < 4; y++)
			{
				for (int x = 0; x < 4; x++)
				{
					int posX = srcX + x;
					int posY = srcY + y;

					int index = posY * stride + (posX * 3);

					uint8 r = srcPixels[index + 0];
					uint8 g = srcPixels[index + 1];
					uint8 b = srcPixels[index + 2];

					uint32 color = 0xFF000000 | ((b << 16) | (g << 8) | (r));

					pixels[y * 4 + x] = color;
				}
			}

			// compress the 4x4 block using DXT1 compression
			squish::Compress( (squish::u8 *)&pixels[0], dstBlock, squish::kDxt1 );
		}
	};


	struct ComplexRunParams : public MT::TaskBase<ComplexRunParams>
	{
		uint32 width;
		uint32 height;
		uint32 stride;
		uint8 * srcPixels;

		uint32 blkWidth;
		uint32 blkHeight;

		uint8 * dstBlocks;

		ComplexRunParams(uint32 _width, uint32 _height, uint32 _stride, uint8* _srcPixels)
		{
			width = _width;
			height = _height;
			stride = _stride;

			srcPixels = _srcPixels;

			blkWidth = width >> 2;
			blkHeight = height >> 2;
		}


		void Do(MT::FiberContext& context)
		{
			// use stack_array as subtask container. beware stack overflow!
			MT::StackArray<ComplexRunBlockSubtask, 1024> subTasks;

			//uint32 blkX = 0;
			//uint32 blkY = 0;

			for (uint32 blkY = 0; blkY < blkHeight; blkY++)
			{
				for (uint32 blkX = 0; blkX < blkWidth; blkX++)
				{
					uint32 blockIndex = blkY * blkWidth + blkX;

					subTasks.PushBack( ComplexRunBlockSubtask(blkX * 4, blkY * 4, stride, srcPixels, &dstBlocks[blockIndex * 8]) );
				}
			}

			context.RunSubtasksAndYield(MT::TaskGroup::GROUP_0, &subTasks[0], subTasks.Size());
		}
	};


	// dxt compressor complex test
	TEST(RunComplexDxtCompress)
	{
		static_assert(ARRAY_SIZE(EmbeddedImage::lenaColor) == 49152, "Image size is invalid");
		static_assert(ARRAY_SIZE(EmbeddedImage::lenaColorDXT1) == 8192, "Image size is invalid");

		ComplexRunParams complexTask(128, 128, 384, (uint8 *)&EmbeddedImage::lenaColor[0]);
		ASSERT ((complexTask.width & 3) == 0 && (complexTask.height & 4) == 0, "Image size must be a multiple of 4");

		int dxtBlocksTotalSize = complexTask.blkWidth * complexTask.blkHeight * 8;
		complexTask.dstBlocks = (uint8 *)malloc( dxtBlocksTotalSize );
		memset(complexTask.dstBlocks, 0x0, dxtBlocksTotalSize);

		MT::TaskScheduler scheduler;

		scheduler.RunAsync(MT::TaskGroup::GROUP_0, &complexTask, 1);

		CHECK(scheduler.WaitAll(3000000));
		CHECK_ARRAY_EQUAL(complexTask.dstBlocks, EmbeddedImage::lenaColorDXT1, dxtBlocksTotalSize);

		free(complexTask.dstBlocks);

#ifdef MT_INSTRUMENTED_BUILD

		MT::ProfileEventDesc eventsBuffer[4096];
		uint32 workerThreadCount = scheduler.GetWorkerCount();
		for(uint32 workerId = 0; workerId < workerThreadCount; workerId++)
		{
			size_t eventsCount = scheduler.GetProfilerEvents(workerId, &eventsBuffer[0], ARRAY_SIZE(eventsBuffer));
			printf("Thread[%d], Profile events count = %d\n", workerId, (uint32)eventsCount);
		}

#endif

	}


}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
