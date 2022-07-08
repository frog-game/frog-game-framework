#include "gtest/gtest.h"

extern "C" {
#include "byteQueue_t.h"
}

TEST(byteQueue, testInit_0)
{
	size_t nReadBytes = 0;
	size_t nWriteBytes = 0;

	byteQueue_tt byteQueue;
	byteQueue_init(&byteQueue,0);
	
	EXPECT_EQ(byteQueue_getCapacity(&byteQueue),0);
	EXPECT_EQ(byteQueue_getBytesReadable(&byteQueue),0);
	EXPECT_EQ(byteQueue_getBytesWritable(&byteQueue),0);
	EXPECT_TRUE(byteQueue_empty(&byteQueue));
	EXPECT_EQ(byteQueue_getBuffer(&byteQueue),nullptr);

	EXPECT_EQ(byteQueue_peekContiguousBytesRead(&byteQueue,&nReadBytes),nullptr);
	EXPECT_EQ(nReadBytes,0);
	EXPECT_EQ(byteQueue_peekContiguousBytesWrite(&byteQueue,&nWriteBytes),nullptr);
	EXPECT_EQ(nWriteBytes,0);

	size_t n = strlen("string test");
	byteQueue_writeBytes(&byteQueue,"string test",n);
	EXPECT_EQ(byteQueue_getBytesReadable(&byteQueue),n);
	EXPECT_EQ(byteQueue_getBytesWritable(&byteQueue),byteQueue_getCapacity(&byteQueue)-n);
	EXPECT_FALSE(byteQueue_empty(&byteQueue));

	char* r = byteQueue_peekContiguousBytesRead(&byteQueue,&nReadBytes);
	EXPECT_EQ(nReadBytes,n);
	EXPECT_EQ(memcmp(r,"string test",nReadBytes),0);

	byteQueue_peekContiguousBytesWrite(&byteQueue,&nWriteBytes);
	EXPECT_EQ(nWriteBytes,byteQueue_getCapacity(&byteQueue)-n);

	char szBuffer[256];
	EXPECT_TRUE(byteQueue_readBytes(&byteQueue,szBuffer,256,false));

	EXPECT_EQ(memcmp(szBuffer,"string test",nReadBytes),0);

	EXPECT_EQ(byteQueue_getBytesReadable(&byteQueue),0);
	EXPECT_EQ(byteQueue_getBytesWritable(&byteQueue),byteQueue_getCapacity(&byteQueue));
	EXPECT_TRUE(byteQueue_empty(&byteQueue));

	byteQueue_peekContiguousBytesRead(&byteQueue,&nReadBytes);
	EXPECT_EQ(nReadBytes,0);

	byteQueue_peekContiguousBytesWrite(&byteQueue,&nWriteBytes);
	EXPECT_EQ(nWriteBytes,byteQueue_getCapacity(&byteQueue));
}


TEST(byteQueue, testInit_256)
{
	size_t nReadBytes = 0;
	size_t nWriteBytes = 0;

	byteQueue_tt byteQueue;
	byteQueue_init(&byteQueue,256);

	EXPECT_EQ(byteQueue_getCapacity(&byteQueue),256);
	EXPECT_EQ(byteQueue_getBytesReadable(&byteQueue),0);
	EXPECT_EQ(byteQueue_getBytesWritable(&byteQueue),256);
	EXPECT_TRUE(byteQueue_empty(&byteQueue));

	byteQueue_peekContiguousBytesRead(&byteQueue,&nReadBytes);
	EXPECT_EQ(nReadBytes,0);

	byteQueue_peekContiguousBytesWrite(&byteQueue,&nWriteBytes);
	EXPECT_EQ(nWriteBytes,256);

	size_t n = strlen("string test");
	byteQueue_writeBytes(&byteQueue,"string test",n);
	EXPECT_EQ(byteQueue_getBytesReadable(&byteQueue),n);
	EXPECT_EQ(byteQueue_getBytesWritable(&byteQueue),256-n);
	EXPECT_FALSE(byteQueue_empty(&byteQueue));

	char* r = byteQueue_peekContiguousBytesRead(&byteQueue,&nReadBytes);
	EXPECT_EQ(nReadBytes,n);

	EXPECT_EQ(memcmp(r,"string test",nReadBytes),0);

	byteQueue_peekContiguousBytesWrite(&byteQueue,&nWriteBytes);
	EXPECT_EQ(nWriteBytes,256-n);

	char szBuffer[256];
	EXPECT_TRUE(byteQueue_readBytes(&byteQueue,szBuffer,256,false));
	EXPECT_EQ(memcmp(szBuffer,"string test",nReadBytes),0);

	EXPECT_EQ(byteQueue_getBytesReadable(&byteQueue),0);
	EXPECT_EQ(byteQueue_getBytesWritable(&byteQueue),256);
	EXPECT_TRUE(byteQueue_empty(&byteQueue));

	byteQueue_peekContiguousBytesRead(&byteQueue,&nReadBytes);
	EXPECT_EQ(nReadBytes,0);

	byteQueue_peekContiguousBytesWrite(&byteQueue,&nWriteBytes);
	EXPECT_EQ(nWriteBytes,256);
}
