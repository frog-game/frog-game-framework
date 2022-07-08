#include "gtest/gtest.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

extern "C" {
#include "time_t.h"
#include "thread_t.h"
}

TEST(time, test_0)
{
	timespec_tt time1;
	timespec_tt time2;
	timespec_tt time3;
	getClockMonotonic(&time1);
	timespec_tt timeSleep;
	timeSleep.iSec = 1;
	timeSleep.iNsec = 0;
	sleep_for(&timeSleep);
	getClockMonotonic(&time2);
	timespec_sub(&time2,&time1,&time3)
	int64_t iTimer1 = timespec_toMsec(&time1);
	int64_t iTimer2 = timespec_toMsec(&time2);
	int64_t iTimer3 = timespec_toMsec(&time3);

	printf("cur:%" PRIi64"ms last:%" PRIi64"  ms sleep:%" PRIi64" ms\n",iTimer1,iTimer2,iTimer3);
	int64_t iTimer1Ns = timespec_toNsec(&time1);
	int64_t iTimer2Ns = timespec_toNsec(&time2);
	int64_t iTimer3Ns = timespec_toNsec(&time3);
	printf("cur:%" PRIi64" ns last:%" PRIi64" ns sleep:%" PRIi64" ns\n",iTimer1Ns,iTimer2Ns,iTimer3Ns);
}

TEST(time, test_1)
{
	timespec_tt time1;
	timespec_tt time2;
	timespec_tt time3;
	getClockRealtime(&time1);
	timespec_tt timeSleep;
	timeSleep.iSec = 1;
	timeSleep.iNsec = 0;
	sleep_for(&timeSleep);
	getClockRealtime(&time2);
	timespec_sub(&time2,&time1,&time3)
	int64_t iTimer1 = timespec_toMsec(&time1);
	int64_t iTimer2 = timespec_toMsec(&time2);
	int64_t iTimer3 = timespec_toMsec(&time3);

	printf("cur:%" PRIi64" ms last:%" PRIi64" ms sleep:%" PRIi64" ms\n",iTimer1,iTimer2,iTimer3);
	int64_t iTimer1Ns = timespec_toNsec(&time1);
	int64_t iTimer2Ns = timespec_toNsec(&time2);
	int64_t iTimer3Ns = timespec_toNsec(&time3);
	printf("cur:%" PRIi64" ns last:%" PRIi64" ns sleep:%" PRIi64" ns\n",iTimer1Ns,iTimer2Ns,iTimer3Ns);
}

TEST(time, test_2)
{
	timespec_tt time = {10000,5000000};
	timespec_tt time2 = {100000,15000000};
	int64_t iTime = timespec_addToMs(&time,&time2);
	ASSERT_EQ(iTime,110000020);
	int64_t iTime2 = timespec_addToNs(&time,&time2);
	ASSERT_EQ(iTime2,110000020000000);
	int64_t iTime3 = timespec_subToMs(&time2,&time);
	ASSERT_EQ(iTime3,90000010);
	int64_t iTime4 = timespec_subToNs(&time2,&time);
	ASSERT_EQ(iTime4,90000010000000);
}

TEST(time, test_3)
{
	timespec_tt time = {10000,5000000};
	timespec_tt time2 = {100000,15000000};
	timespec_tt time3 = {0,0};
	timespec_add(&time,&time2,&time3);
	int64_t iTime = timespec_toMsec(&time3);
	ASSERT_EQ(iTime,110000020);
	int64_t iTime2 = timespec_toNsec(&time3);
	ASSERT_EQ(iTime2,110000020000000);
	timespec_sub(&time2,&time,&time3);
	int64_t iTime3 = timespec_toMsec(&time3);
	ASSERT_EQ(iTime3,90000010);
	int64_t iTime4 = timespec_toNsec(&time3);
	ASSERT_EQ(iTime4,90000010000000);
}
