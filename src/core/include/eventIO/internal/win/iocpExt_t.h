

#pragma once

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <stdatomic.h>

#include "utility_t.h"

typedef LONG NTSTATUS;

#ifndef NT_SUCCESS
#    define NT_SUCCESS(status) ((NTSTATUS)(status) >= 0)
#endif

#define def_IOCP_QUEUED_QUIT 0
#define def_IOCP_CONNECTION 1
#define def_IOCP_ACCEPT 2
#define def_IOCP_RECVFROM 3
#define def_IOCP_EVENT 4
#define def_IOCP_TASK 5

typedef BOOL(WINAPI* AcceptExPtr)(SOCKET, SOCKET, PVOID, DWORD, DWORD, DWORD, LPDWORD,
                                  LPOVERLAPPED);
typedef void(WINAPI* GetAcceptExSockaddrsPtr)(PVOID, DWORD, DWORD, DWORD, LPSOCKADDR*, LPINT,
                                              LPSOCKADDR*, LPINT);
typedef BOOL(WINAPI* ConnectExPtr)(SOCKET, const struct sockaddr*, int, PVOID, DWORD, LPDWORD,
                                   LPOVERLAPPED);
typedef BOOL(WINAPI* DisconnectExPtr)(SOCKET, LPOVERLAPPED, DWORD, DWORD);
typedef BOOL(WINAPI* GetQueuedCompletionStatusExPtr)(HANDLE, LPOVERLAPPED_ENTRY, ULONG, PULONG,
                                                     DWORD, BOOL);
typedef BOOL(WINAPI* CancelIoExPtr)(HANDLE, LPOVERLAPPED);

__UNUSED BOOL acceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, PVOID lpOutputBuffer,
                       DWORD dwReceiveDataLength, DWORD dwLocalAddressLength,
                       DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived,
                       LPOVERLAPPED lpOverlapped);

__UNUSED void getAcceptExSockaddrs(PVOID lpOutputBuffer, DWORD dwReceiveDataLength,
                                   DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength,
                                   LPSOCKADDR* LocalSockaddr, LPINT LocalSockaddrLength,
                                   LPSOCKADDR* RemoteSockaddr, LPINT RemoteSockaddrLength);

__UNUSED BOOL connectEx(SOCKET s, const struct sockaddr* name, int namelen, PVOID lpSendBuffer,
                        DWORD dwSendDataLength, LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped);

__UNUSED BOOL disconnectEx(SOCKET hSocket, LPOVERLAPPED lpOverlapped, DWORD dwFlags,
                           DWORD reserved);

__UNUSED BOOL getQueuedCompletionStatusEx(HANDLE             CompletionPort,
                                          LPOVERLAPPED_ENTRY lpCompletionPortEntries, ULONG ulCount,
                                          PULONG ulNumEntriesRemoved, DWORD dwMilliseconds,
                                          BOOL bAlertable);

__UNUSED BOOL cancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped);