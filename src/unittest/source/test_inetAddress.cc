#include "gtest/gtest.h"

#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include "inetAddress_t.h"
}

TEST(inetAddress, ipv4_test_0)
{
	inetAddress_tt inetAddress;
	inetAddress_init_fromIpPort(&inetAddress,"127.0.0.1:9978");
	EXPECT_EQ(inetAddress_getNetworkPort(&inetAddress),htons(9978));
	EXPECT_EQ(inetAddress_getPort(&inetAddress),9978);
	printf("ip:%x\n",inetAddress_getNetworkIP(&inetAddress));
	
	char szBuffer[64];
	EXPECT_TRUE(inetAddress_toIPString(&inetAddress,szBuffer,64));
	EXPECT_STREQ(szBuffer,"127.0.0.1");
	EXPECT_TRUE(inetAddress_isIpV4(&inetAddress));

	char szBuffer2[64];
	EXPECT_TRUE(inetAddress_toIPPortString(&inetAddress,szBuffer2,64));
	EXPECT_STREQ(szBuffer2,"127.0.0.1:9978");
}

TEST(inetAddress, ipv4_test_1)
{
	inetAddress_tt inetAddress;
	inetAddress_init_fromIpPort(&inetAddress,"192.168.1.25:9978");
	EXPECT_EQ(inetAddress_getNetworkPort(&inetAddress),htons(9978));
	EXPECT_EQ(inetAddress_getPort(&inetAddress),9978);
	printf("ip:%x\n",inetAddress_getNetworkIP(&inetAddress));
	
	char szBuffer[64];
	EXPECT_TRUE(inetAddress_toIPString(&inetAddress,szBuffer,64));
	EXPECT_STREQ(szBuffer,"192.168.1.25");
	EXPECT_TRUE(inetAddress_isIpV4(&inetAddress));
	
	char szBuffer2[64];
	EXPECT_TRUE(inetAddress_toIPPortString(&inetAddress,szBuffer2,64));
	EXPECT_STREQ(szBuffer2,"192.168.1.25:9978");
}

TEST(inetAddress, ipv6_test_0)
{
	inetAddress_tt inetAddress;
	inetAddress_init_fromIpPort(&inetAddress,"abcd:ef01:2345:6789:abcd:ef01:2345:6789:9978");
	EXPECT_EQ(inetAddress_getNetworkPort(&inetAddress),htons(9978));
	EXPECT_EQ(inetAddress_getPort(&inetAddress),9978);

	char szBuffer[64];
	EXPECT_TRUE(inetAddress_toIPString(&inetAddress,szBuffer,64));
	EXPECT_STREQ(szBuffer,"abcd:ef01:2345:6789:abcd:ef01:2345:6789");
	EXPECT_TRUE(inetAddress_isIpV6(&inetAddress));

	char szBuffer2[64];
	EXPECT_TRUE(inetAddress_toIPPortString(&inetAddress,szBuffer2,64));
	EXPECT_STREQ(szBuffer2,"abcd:ef01:2345:6789:abcd:ef01:2345:6789:9978");
}

TEST(inetAddress, ipv6_test_1)
{
	inetAddress_tt inetAddress;
	inetAddress_init_fromIpPort(&inetAddress,"[abcd:ef01:2345:6789:abcd:ef01:2345:6789]:9978");
	EXPECT_EQ(inetAddress_getNetworkPort(&inetAddress),htons(9978));
	EXPECT_EQ(inetAddress_getPort(&inetAddress),9978);

	char szBuffer[64];
	EXPECT_TRUE(inetAddress_toIPString(&inetAddress,szBuffer,64));
	EXPECT_STREQ(szBuffer,"abcd:ef01:2345:6789:abcd:ef01:2345:6789");
	EXPECT_TRUE(inetAddress_isIpV6(&inetAddress));

	char szBuffer2[64];
	EXPECT_TRUE(inetAddress_toIPPortString(&inetAddress,szBuffer2,64));
	EXPECT_STREQ(szBuffer2,"abcd:ef01:2345:6789:abcd:ef01:2345:6789:9978");
}