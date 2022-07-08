#include "gtest/gtest.h"

#include <stdio.h>
#include <stdlib.h>
#include <atomic>
#include <inttypes.h>

extern "C" {
#include "utility_t.h"
#include "time_t.h"
#include "thread_t.h"
#include "eventIO/eventIO_t.h"
#include "eventIO/eventIOThread_t.h"
}


TEST(eventIO, test_0)
{
	eventIO_tt* pEventIO = createEventIO();
	eventIO_start(pEventIO,true);
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	eventIOThread_stop(pEventIOThread,true);
	eventIO_release(pEventIO);
}

void watcher1(eventWatcher_tt* pEventWatcher,void* pData)
{
	ASSERT_NE(pData,nullptr);
	int32_t* pTestData = (int32_t*)pData;
	EXPECT_EQ(*pTestData ,8);
}

void watcherClose1(void* pData)
{
	ASSERT_NE(pData,nullptr);
	int32_t* pTestData = (int32_t*)pData;
	EXPECT_EQ(*pTestData ,8);
	free(pData);
}

TEST(eventIO, test_1)
{
	eventIO_tt* pEventIO = createEventIO();
	eventIO_start(pEventIO,true);
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	int32_t* pTestData = (int32_t*)malloc(sizeof(int32_t));
	*pTestData = 8;
	eventWatcher_tt* pTest = createEventWatcher(pEventIO,true,watcher1, pTestData,watcherClose1);
	eventWatcher_start(pTest);
	eventWatcher_notify(pTest);
	timespec_tt timeSleep;
	timeSleep.iSec = 1;
	timeSleep.iNsec = 0;
	sleep_for(&timeSleep);
	
	eventWatcher_close(pTest);
	eventWatcher_release(pTest);
	pTest = NULL;
	eventIOThread_stop(pEventIOThread,true);
	eventIO_release(pEventIO);
}

static timespec_tt time1;

void onTimer1(eventTimer_tt* p,void* pData)
{
	timespec_tt time2;
	timespec_tt time3;
	getClockMonotonic(&time2);
	timespec_sub(&time2,&time1,&time3)

	uint64_t uiTimer1 = timespec_toMsec(&time1);
	uint64_t uiTimer2 = timespec_toMsec(&time2);
	uint64_t uiTimer3 = timespec_toMsec(&time3);

	printf("onTimer cur:%" PRIu64" ms last:%" PRIu64" ms sleep:%" PRIu64" ms\n",uiTimer1,uiTimer2,uiTimer3);


	eventIOThread_tt* pEventIOThread = (eventIOThread_tt*)pData;
	eventIOThread_stop(pEventIOThread,false);
}


TEST(eventIO, test_2)
{
	eventIO_tt* pEventIO = createEventIO();
	eventIO_start(pEventIO,false);
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	eventTimer_tt* pEventTimer = createEventTimer(pEventIO,onTimer1,true, 1000, pEventIOThread);
	getClockMonotonic(&time1);
	ASSERT_TRUE(eventTimer_start(pEventTimer));
	ASSERT_TRUE(eventTimer_isOnce(pEventTimer));
	eventTimer_release(pEventTimer);
	pEventTimer = NULL;
	eventIOThread_join(pEventIOThread);
	eventIO_release(pEventIO);
}

static int32_t iTimerCount = 0;

void onTimer2(eventTimer_tt* p,void* pData)
{
	++iTimerCount;
	printf("onTimer2 %d\n",iTimerCount);
	if(iTimerCount == 10 )
	{
		printf("onTimer2 stop\n");
		eventTimer_stop(p);
	}
}

void onTimerStop2(eventTimer_tt* p,void* pData)
{
	eventIOThread_tt* pEventIOThread = (eventIOThread_tt*)pData;

	eventIOThread_stop(pEventIOThread,false);
}

TEST(eventIO, test_3)
{
	iTimerCount = 0;
	eventIO_tt* pEventIO = createEventIO();
	eventIO_start(pEventIO,false);
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	eventTimer_tt* pEventTimer = createEventTimer(pEventIO,onTimer2,false, 100, pEventIOThread);
	eventTimer_setCloseCallback(pEventTimer,onTimerStop2);
	ASSERT_TRUE(eventTimer_start(pEventTimer));
	ASSERT_TRUE(!eventTimer_isOnce(pEventTimer));
	eventIOThread_join(pEventIOThread);
	eventTimer_release(pEventTimer);
	pEventTimer = NULL;
	eventIO_release(pEventIO);
}

static std::atomic_int  	s_iAcceptIndex;
static std::atomic_int  	s_iExitlistenIndex;
static eventListenPort_tt* 	s_pListen;

typedef struct eventTestData_s
{
	eventConnection_tt* pEventConnection;
	int32_t				iIndex;
	int32_t             iPacketIndex;
} eventTestData_tt;

bool clientReceiveCallback(eventConnection_tt* pHandle,byteQueue_tt* pByteQueue,void* pData)
{
	eventTestData_tt* pTestData = (eventTestData_tt*)pData;
	char szBuffer[256];
	size_t nBytesWritten = byteQueue_getBytesReadable(pByteQueue);
	byteQueue_readBytes(pByteQueue,szBuffer, nBytesWritten,false);

	char szCmpBuffer[256];
	sprintf(szCmpBuffer, "send test:[%d]", pTestData->iPacketIndex);
	if(memcmp(szBuffer,szCmpBuffer,nBytesWritten) != 0)
	{
		printf("ConnectionReceive error\n");
		return false;
	}

	if(pTestData->iPacketIndex < 8000)
	{
		++pTestData->iPacketIndex;
		char szBuffer[256];
		eventTestData_tt* pTestData = (eventTestData_tt*)pData;
		sprintf(szBuffer, "send test:[%d]", pTestData->iPacketIndex);
		eventConnection_send(pHandle,createEventBuf(szBuffer,strlen(szBuffer), NULL, 0));
		return true;
	}
	return true;
}


bool serverReceiveCallback(eventConnection_tt* pHandle,byteQueue_tt* pByteQueue,void* pData)
{
	eventTestData_tt* pTestData  = (eventTestData_tt*)pData;
	char szBuffer[256];
	size_t nBytesWritten = byteQueue_getBytesReadable(pByteQueue);
	byteQueue_readBytes(pByteQueue,szBuffer, nBytesWritten,false);

	char szCmpBuffer[256];
	sprintf(szCmpBuffer, "send test:[%d]", pTestData->iPacketIndex);
	if(pTestData->iPacketIndex <= 8000)
	{
		if(memcmp(szBuffer,szCmpBuffer,nBytesWritten) != 0)
		{
			szBuffer[nBytesWritten] = '\0';
			printf("serverReceive error recv:%s,dst:%s\n",szBuffer,szCmpBuffer);

			inetAddress_tt address;
			eventConnection_getRemoteAddr(pHandle,&address);
			char szAddrBuffer[64];
			EXPECT_TRUE(inetAddress_toIPPortString(&address,szAddrBuffer,64));
			printf("serverReceive error addr:%s\n",szAddrBuffer);
			
			return false;
		}

		++pTestData->iPacketIndex;
		eventConnection_send(pHandle, createEventBuf(szBuffer, nBytesWritten, NULL, 0));
		if (pTestData->iPacketIndex <= 8000)
		{
			return true;
		}
	}
	printf("ConnectionReceive ok\n");

	inetAddress_tt address;
	eventConnection_getRemoteAddr(pHandle,&address);
	eventConnection_close(pHandle);
	eventConnection_release(pHandle);
	char szAddrBuffer[64];
	EXPECT_TRUE(inetAddress_toIPPortString(&address,szAddrBuffer,64));
	printf("Disconnect addr:%s\n",szAddrBuffer);
	if(std::atomic_fetch_sub(&s_iExitlistenIndex,1) == 1)
	{
		printf("s_pListen close\n");
		if(s_pListen)
		{
			eventListenPort_close(s_pListen);
			eventListenPort_release(s_pListen);
			s_pListen = NULL;
		}
	}

	return true;
}

void serverDisconnectCallback(eventConnection_tt* pHandle,void* pData)
{
	inetAddress_tt address;
	eventConnection_getRemoteAddr(pHandle,&address);
	char szAddrBuffer[64];
	EXPECT_TRUE(inetAddress_toIPPortString(&address,szAddrBuffer,64));
	printf("Disconnect addr:%s\n",szAddrBuffer);

	eventConnection_forceClose(pHandle);
	eventConnection_release(pHandle);
	if(std::atomic_fetch_sub(&s_iExitlistenIndex,1) == 1)
	{
		printf("s_pListen close\n");
		if(s_pListen)
		{
			eventListenPort_close(s_pListen);
			eventListenPort_release(s_pListen);
			s_pListen = NULL;
		}
	}
}   

void clientDisconnectCallback(eventConnection_tt* pHandle,void* pData)
{
	printf("clientDisconnectCallback \n");
	eventConnection_forceClose(pHandle);
	eventConnection_release(pHandle);
}

void fnUserFree(void* pUserData)
{
	free(pUserData);
}

void eventListenPortAcceptCallback(eventListenPort_tt* pHandle,eventConnection_tt* pConnection,const char* pBuffer,uint32_t uiLength,void* pData)
{
	if (pConnection == NULL)
	{
		return;
	}
	int32_t iIndex = std::atomic_fetch_add(&s_iAcceptIndex,1);

	eventTestData_tt* pTestData = (eventTestData_tt*)malloc(sizeof(eventTestData_tt));
	pTestData->iIndex = iIndex;
	pTestData->iPacketIndex = 0;
	pTestData->pEventConnection = pConnection;

	eventConnection_setReceiveCallback(pConnection, serverReceiveCallback);
	eventConnection_setDisconnectCallback(pConnection, serverDisconnectCallback);

	inetAddress_tt address;
	eventConnection_getRemoteAddr(pConnection,&address);
	char szAddrBuffer[64];
	EXPECT_TRUE(inetAddress_toIPPortString(&address,szAddrBuffer,64));
	printf("accept addr:%s\n",szAddrBuffer);

	if(uiLength !=0)
	{
		char szCmpBuffer[256];
		sprintf(szCmpBuffer, "send test:[%d]", pTestData->iPacketIndex);
		if(memcmp(pBuffer,szCmpBuffer,uiLength) != 0)
		{
			printf("eventListenPortAcceptCallback error\n");
			char szBuffer[256];
			memcpy(szBuffer,pBuffer,uiLength);
			szBuffer[uiLength] = '\0';
			printf("Accept error addr:%s recv:%s,dst:%s\n",szAddrBuffer,szBuffer,szCmpBuffer);
			return;
		}
		++pTestData->iPacketIndex;
		ASSERT_TRUE(eventConnection_bind(pConnection,true,true,pTestData,fnUserFree));
		eventConnection_send(pConnection,createEventBuf(pBuffer,uiLength, NULL, 0));
		return;
	}
	else
	{
		ASSERT_TRUE(eventConnection_bind(pConnection,true,true,pTestData,fnUserFree));
	}
}

void listenPortFreeCallback(void* pData)
{
	printf("listenPortFreeCallback\n");
	ASSERT_NE(pData,nullptr);
	eventIOThread_tt* pEventIOThread = (eventIOThread_tt*)pData;
	eventIOThread_stop(pEventIOThread,false);
}

void eventConnectionConnectorCallback(eventConnection_tt* pHandle, void* pData)
{
	if(!eventConnection_isConnecting(pHandle))
	{
		if (pHandle)
		{
			eventConnection_release(pHandle);
		}
		FAIL()<<"Connector Fail";
	}
	else
	{
		eventConnection_setReceiveCallback(pHandle, clientReceiveCallback);
		eventConnection_setDisconnectCallback(pHandle, clientDisconnectCallback);
		ASSERT_TRUE(eventConnection_bind(pHandle,true,true,pData,NULL));

		inetAddress_tt address;
		eventConnection_getLocalAddr(pHandle,&address);
		char szAddrBuffer[64];
		EXPECT_TRUE(inetAddress_toIPPortString(&address,szAddrBuffer,64));
		printf("Connector local addr:%s\n",szAddrBuffer);

		char szBuffer[256];
		eventTestData_tt* pTestData = (eventTestData_tt*)pData;
		sprintf(szBuffer, "send test:[%d]", pTestData->iPacketIndex);
		eventConnection_send(pHandle,createEventBuf(szBuffer,strlen(szBuffer), NULL, 0));
	}
}


TEST(eventIO, test_4)
{
	std::atomic_init(&s_iAcceptIndex,0);
	std::atomic_init(&s_iExitlistenIndex,32);

	eventIO_tt* pEventIO = createEventIO();
	eventIO_start(pEventIO,false);
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	inetAddress_tt address;
	inetAddress_init(&address,"127.0.0.1",4430,false);

	s_pListen = createEventListenPort(pEventIO,&address,true);
	eventListenPort_setAcceptCallback(s_pListen, eventListenPortAcceptCallback);
	ASSERT_TRUE(eventListenPort_start(s_pListen,pEventIOThread,listenPortFreeCallback));

	timespec_tt timeSleep;
	timeSleep.iSec = 1;
	timeSleep.iNsec = 0;
	sleep_for(&timeSleep);

	// timespec_tt timeSleep2;
	// timeSleep2.iSec = 0;
	// timeSleep2.iNsec = 1000;

	eventTestData_tt   eventConnectTestData[32];
	
	for(int32_t i = 0; i <32; ++i)
	{
		eventTestData_tt* pTestData = &(eventConnectTestData[i]);
		pTestData->iIndex = i;
		pTestData->iPacketIndex = 0;
		pTestData->pEventConnection = createEventConnection(pEventIO,&address,true);
		eventConnection_setConnectorCallback(pTestData->pEventConnection, eventConnectionConnectorCallback);
		ASSERT_TRUE(eventConnection_connect(pTestData->pEventConnection,pTestData,NULL));
		//sleep_for(&timeSleep2);
	}

	eventIOThread_join(pEventIOThread);
	eventIO_release(pEventIO);
	pEventIO = NULL;
}


TEST(eventIO, test_5)
{
	std::atomic_init(&s_iAcceptIndex,0);
	std::atomic_init(&s_iExitlistenIndex,32);

	eventIO_tt* pEventIO = createEventIO();
	eventIO_start(pEventIO,false);
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	inetAddress_tt address;
	inetAddress_init(&address,"127.0.0.1",4431,false);

	s_pListen = createEventListenPort(pEventIO,&address,false);
	eventListenPort_setAcceptCallback(s_pListen, eventListenPortAcceptCallback);
	ASSERT_TRUE(eventListenPort_start(s_pListen,pEventIOThread,listenPortFreeCallback));

	timespec_tt timeSleep;
	timeSleep.iSec = 1;
	timeSleep.iNsec = 0;
	sleep_for(&timeSleep);

	eventTestData_tt   eventConnectTestData[32];
	
	// timespec_tt timeSleep2;
	// timeSleep2.iSec = 0;
	// timeSleep2.iNsec = 1000;

	for(int32_t i = 0; i <32; ++i)
	{
		eventTestData_tt* pTestData = &(eventConnectTestData[i]);
		pTestData->iIndex = i;
		pTestData->iPacketIndex = 0;
		pTestData->pEventConnection = createEventConnection(pEventIO,&address,false);
		eventConnection_setConnectorCallback(pTestData->pEventConnection, eventConnectionConnectorCallback);
		// eventConnection_setReceiveCallback(pTestData->pEventConnection, clientReceiveCallback);
		// eventConnection_setDisconnectCallback(pTestData->pEventConnection, clientDisconnectCallback);
		ASSERT_TRUE(eventConnection_connect(pTestData->pEventConnection,pTestData,NULL));
		// ASSERT_TRUE(eventConnection_bind(pTestData->pEventConnection,false, false,pTestData,NULL));
		// char szBuffer[256];
		// sprintf(szBuffer, "send test:[%d]", pTestData->iPacketIndex);
		// eventConnection_send(pTestData->pEventConnection,eventConnectionWrite_init(szBuffer,strlen(szBuffer), NULL, 0));
		//sleep_for(&timeSleep2);
	}

	eventIOThread_join(pEventIOThread);
	eventIO_release(pEventIO);
	pEventIO = NULL;
}

TEST(eventIO, multithreadingTest_0)
{
	eventIO_tt* pEventIO = createEventIO();
	eventIO_setConcurrentThreads(pEventIO,threadHardwareConcurrency());
	ASSERT_TRUE(eventIO_start(pEventIO,true));
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	eventIOThread_stop(pEventIOThread,true);
	eventIO_release(pEventIO);
}

TEST(eventIO, multithreadingTest_1)
{
	eventIO_tt* pEventIO = createEventIO();
	eventIO_setConcurrentThreads(pEventIO,threadHardwareConcurrency());
	ASSERT_TRUE(eventIO_start(pEventIO,true));
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	int32_t* pTestData = (int32_t*)malloc(sizeof(int32_t));
	*pTestData = 8;
	eventWatcher_tt* pTest = createEventWatcher(pEventIO,true,watcher1, pTestData,watcherClose1);
	eventWatcher_start(pTest);
	eventWatcher_notify(pTest);
	timespec_tt timeSleep;
	timeSleep.iSec = 1;
	timeSleep.iNsec = 0;
	sleep_for(&timeSleep);
	
	eventWatcher_close(pTest);
	eventWatcher_release(pTest);
	pTest = NULL;
	eventIOThread_stop(pEventIOThread,true);
	eventIO_release(pEventIO);
}

TEST(eventIO, multithreadingTest_2)
{
	eventIO_tt* pEventIO = createEventIO();
	eventIO_setConcurrentThreads(pEventIO,threadHardwareConcurrency());
	ASSERT_TRUE(eventIO_start(pEventIO,false));
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	eventTimer_tt* pEventTimer = createEventTimer(pEventIO,onTimer1,true, 1000, pEventIOThread);
	getClockMonotonic(&time1);
	ASSERT_TRUE(eventTimer_start(pEventTimer));
	ASSERT_TRUE(eventTimer_isOnce(pEventTimer));
	eventTimer_release(pEventTimer);
	pEventTimer = NULL;
	eventIOThread_join(pEventIOThread);
	eventIO_release(pEventIO);
}

TEST(eventIO, multithreadingTest_3)
{
	iTimerCount = 0;
	eventIO_tt* pEventIO = createEventIO();
	eventIO_setConcurrentThreads(pEventIO,threadHardwareConcurrency());
	ASSERT_TRUE(eventIO_start(pEventIO,false));
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	//printf("eventIOThread_start\n");
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	//printf("eventIOThread_start 2\n");
	eventTimer_tt* pEventTimer = createEventTimer(pEventIO,onTimer2,false, 100, pEventIOThread);
	eventTimer_setCloseCallback(pEventTimer,onTimerStop2);
	ASSERT_TRUE(eventTimer_start(pEventTimer));
	//printf("eventIOThread_start 3\n");
	ASSERT_TRUE(!eventTimer_isOnce(pEventTimer));
	eventIOThread_join(pEventIOThread);
	eventTimer_release(pEventTimer);
	pEventTimer = NULL;
	eventIO_release(pEventIO);
}


TEST(eventIO, multithreadingTest_4)
{
	std::atomic_init(&s_iAcceptIndex,0);
	std::atomic_init(&s_iExitlistenIndex,32);

	eventIO_tt* pEventIO = createEventIO();
	eventIO_setConcurrentThreads(pEventIO,threadHardwareConcurrency());
	eventIO_start(pEventIO,false);
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	inetAddress_tt address;
	inetAddress_init(&address,"127.0.0.1",4432,false);

	s_pListen = createEventListenPort(pEventIO,&address,true);
	eventListenPort_setAcceptCallback(s_pListen, eventListenPortAcceptCallback);
	ASSERT_TRUE(eventListenPort_start(s_pListen,pEventIOThread,listenPortFreeCallback));

	timespec_tt timeSleep;
	timeSleep.iSec = 1;
	timeSleep.iNsec = 0;
	sleep_for(&timeSleep);

	// timespec_tt timeSleep2;
	// timeSleep2.iSec = 0;
	// timeSleep2.iNsec = 1000;

	eventTestData_tt   eventConnectTestData[32];
	
	for(int32_t i = 0; i <32; ++i)
	{
		eventTestData_tt* pTestData = &(eventConnectTestData[i]);
		pTestData->iIndex = i;
		pTestData->iPacketIndex = 0;
		pTestData->pEventConnection = createEventConnection(pEventIO,&address,true);
		eventConnection_setConnectorCallback(pTestData->pEventConnection, eventConnectionConnectorCallback);
		ASSERT_TRUE(eventConnection_connect(pTestData->pEventConnection,pTestData,NULL));
		//sleep_for(&timeSleep2);
	}

	eventIOThread_join(pEventIOThread);
	eventIO_release(pEventIO);
	pEventIO = NULL;
}


TEST(eventIO, multithreadingTest_5)
{
	std::atomic_init(&s_iAcceptIndex,0);
	std::atomic_init(&s_iExitlistenIndex,32);

	eventIO_tt* pEventIO = createEventIO();
	eventIO_setConcurrentThreads(pEventIO,threadHardwareConcurrency());
	eventIO_start(pEventIO,false);
	eventIOThread_tt* pEventIOThread = createEventIOThread(pEventIO);
	eventIOThread_start(pEventIOThread, true,NULL,NULL);
	inetAddress_tt address;
	inetAddress_init(&address,"127.0.0.1",4433,false);

	s_pListen = createEventListenPort(pEventIO,&address,false);
	eventListenPort_setAcceptCallback(s_pListen, eventListenPortAcceptCallback);
	ASSERT_TRUE(eventListenPort_start(s_pListen,pEventIOThread,listenPortFreeCallback));

	timespec_tt timeSleep;
	timeSleep.iSec = 1;
	timeSleep.iNsec = 0;
	sleep_for(&timeSleep);

	// timespec_tt timeSleep2;
	// timeSleep2.iSec = 0;
	// timeSleep2.iNsec = 1000;

	eventTestData_tt   eventConnectTestData[32];
	
	for(int32_t i = 0; i <32; ++i)
	{
		eventTestData_tt* pTestData = &(eventConnectTestData[i]);
		pTestData->iIndex = i;
		pTestData->iPacketIndex = 0;
		pTestData->pEventConnection = createEventConnection(pEventIO,&address,false);
		eventConnection_setConnectorCallback(pTestData->pEventConnection, eventConnectionConnectorCallback);
		// eventConnection_setReceiveCallback(pTestData->pEventConnection, clientReceiveCallback);
		// eventConnection_setDisconnectCallback(pTestData->pEventConnection, clientDisconnectCallback);
		ASSERT_TRUE(eventConnection_connect(pTestData->pEventConnection,pTestData,NULL));
		// ASSERT_TRUE(eventConnection_bind(pTestData->pEventConnection,false, false,pTestData,NULL));
		// char szBuffer[256];
		// sprintf(szBuffer, "send test:[%d]", pTestData->iPacketIndex);
		// eventConnection_send(pTestData->pEventConnection,eventConnectionWrite_init(szBuffer,strlen(szBuffer), NULL, 0));
		//sleep_for(&timeSleep2);
	}

	eventIOThread_join(pEventIOThread);
	eventIO_release(pEventIO);
	pEventIO = NULL;
}