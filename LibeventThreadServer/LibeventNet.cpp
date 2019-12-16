

#include "LibeventNet.h"

#if defined(__WINDOWS__)
#include <WS2tcpip.h>
#include<WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#ifndef LIBEVENT_SRC
#pragma comment(lib, "event.lib")
#pragma comment(lib, "event_core.lib")
#endif
#elif defined(__LINUX__)
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "event2/event.h"
#include "event2/bufferevent_struct.h"
#include <string.h>
#include <atomic>
#include <iostream>
using namespace std;



//1048576 = 1024 * 1024
#define BUFFER_MAX_READ 1048576

void LibeventNet::event_fatal_cb(int err)
{

}
void LibeventNet::conn_writecb(struct bufferevent* bev, void* user_data)
{

	//  struct evbuffer *output = bufferevent_get_output(bev);
}

void LibeventNet::conn_eventcb(struct bufferevent* bev, short events, void* user_data)
{
	NetObject* pObject = (NetObject*)user_data;
	LibeventNet* pNet = (LibeventNet*)pObject->GetNet();
	std::cout << "Thread ID = " << std::this_thread::get_id() << " FD = " << pObject->GetRealFD() << " Event ID =" << events << std::endl;

	if (events & BEV_EVENT_CONNECTED)
	{
		//must to set it's state before the "EventCB" functional be called[maybe user will send msg in the callback function]
		pNet->mbWorking = true;
	}
	else
	{
		if (!pNet->mbServer)
		{
			pNet->mbWorking = false;
		}
	}

	if (events & BEV_EVENT_CONNECTED)
	{
		struct evbuffer* input = bufferevent_get_input(bev);
		struct evbuffer* output = bufferevent_get_output(bev);
		if (pNet->mnBufferSize > 0)
		{
			evbuffer_expand(input, pNet->mnBufferSize);
			evbuffer_expand(output, pNet->mnBufferSize);
		}
		//printf("%d Connection successed\n", pObject->GetFd());/*XXX win32*/
	}
	else
	{
		pNet->CloseNetObject(pObject->GetRealFD());
	}
}

void LibeventNet::listener_cb(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* sa, int socklen, void* user_data)
{

	LibeventNet* pNet = (LibeventNet*)user_data;
	bool bClose = pNet->CloseNetObject(fd);
	if (bClose)
	{
		return;
	}

	if (pNet->mmObject.size() >= pNet->mnMaxConnect)
	{

		return;
	}

	struct event_base* mxBase = pNet->mxBase;

	struct bufferevent* bev = bufferevent_socket_new(mxBase, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev)
	{
		fprintf(stderr, "Error constructing bufferevent!");
		return;
	}

	struct sockaddr_in* pSin = (sockaddr_in*)sa;

	NetObject* pObject = new NetObject(pNet, fd, *pSin, bev);
	pObject->GetNet()->AddNetObject(fd, pObject);

#if !defined(__WINDOWS__)
	int optval = 1;
	int result = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
	//setsockopt(fd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval));
	if (result < 0)
	{
		std::cout << "setsockopt TCP_NODELAY ERROR !!!" << std::endl;
	}
#endif

	bufferevent_setcb(bev, conn_readcb, conn_writecb, conn_eventcb, (void*)pObject);

	bufferevent_enable(bev, EV_READ | EV_WRITE | EV_CLOSED | EV_TIMEOUT | EV_PERSIST);

	event_set_fatal_callback(event_fatal_cb);

	conn_eventcb(bev, BEV_EVENT_CONNECTED, (void*)pObject);

	bufferevent_set_max_single_read(bev, BUFFER_MAX_READ);
	bufferevent_set_max_single_write(bev, BUFFER_MAX_READ);
}

void LibeventNet::conn_readcb(struct bufferevent* bev, void* user_data)
{
	NetObject* pObject = (NetObject*)user_data;
	if (!pObject)
	{
		return;
	}

	LibeventNet* pNet = (LibeventNet*)pObject->GetNet();
	if (!pNet)
	{
		return;
	}

	if (pObject->NeedRemove())
	{
		return;
	}

	struct evbuffer* input = bufferevent_get_input(bev);
	if (!input)
	{
		return;
	}

	while (1)
	{
		size_t len = evbuffer_get_length(input);
		unsigned char* pData = evbuffer_pullup(input, len);
		pObject->AddBuff((const char*)pData, len);
		int success = evbuffer_drain(input, len);
		if (success == -1)
		{
			//log
		}
		if (len == 0)
		{
			break;
		}
	}

	while (1)
	{
		if (!pNet->Dismantle(pObject))
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool LibeventNet::Execute()
{
	ExecuteClose();

	if (mxBase)
	{
		event_base_loop(mxBase, EVLOOP_ONCE | EVLOOP_NONBLOCK);
	}

	return true;
}

void LibeventNet::InitNet(const char* strIP, const unsigned short nPort)
{
#if defined(__WINDOWS__)
	evthread_use_windows_threads();
#elif defined(__LINUX__)
	evthread_use_pthreads();
#endif
	mstrIP = strIP;
	mnPort = nPort;

	InitClientNet();
}

int LibeventNet::InitNet(const unsigned short nPort, const unsigned int nMaxClient)
{
#if defined(__WINDOWS__)
	evthread_use_windows_threads();
#elif defined(__LINUX__)
	evthread_use_pthreads();
#endif
	mnMaxConnect = nMaxClient;
	mnPort = nPort;

	return InitServerNet();
}

int LibeventNet::InitNet(const char* strIP, const unsigned short nPort, const unsigned int nMaxClient)
{
#if defined(__WINDOWS__)
	evthread_use_windows_threads();
#elif defined(__LINUX__)
	evthread_use_pthreads();
#endif
	mnMaxConnect = nMaxClient;
	mnPort = nPort;
	mstrIP = strIP;

	return InitServerNet();
}

int LibeventNet::ExpandBufferSize(const unsigned int size)
{
	if (size > 0)
	{
		mnBufferSize = size;
	}
	return mnBufferSize;
}

bool LibeventNet::KickAll()
{
	return CloseSocketAll();
}

bool LibeventNet::Final()
{

	CloseSocketAll();

	if (listener)
	{
		evconnlistener_free(listener);
		listener = NULL;
	}

	if (mxBase)
	{
		event_base_free(mxBase);
		mxBase = NULL;
	}

	return true;
}

bool LibeventNet::SendMsgToAllClient(const char* msg, const size_t nLen)
{
	if (nLen <= 0)
	{
		return false;
	}

	if (!mbWorking)
	{
		return false;
	}

	std::map<UINT64, NetObject*>::iterator it = mmObject.begin();
	for (; it != mmObject.end(); ++it)
	{
		NetObject* pNetObject = (NetObject*)it->second;
		if (pNetObject && !pNetObject->NeedRemove())
		{
			bufferevent* bev = (bufferevent*)pNetObject->GetUserData();
			if (NULL != bev)
			{
				bufferevent_write(bev, msg, nLen);

				mnSendMsgTotal++;
			}
		}
	}

	return true;
}

bool LibeventNet::SendMsg(const char* msg, const size_t nLen, const UINT64 nSockIndex)
{
	if (nLen <= 0)
	{
		return false;
	}

	if (!mbWorking)
	{
		return false;
	}

	std::map<UINT64, NetObject*>::iterator it = mmObject.find(nSockIndex);
	if (it != mmObject.end())
	{
		NetObject* pNetObject = (NetObject*)it->second;
		if (pNetObject)
		{
			bufferevent* bev = (bufferevent*)pNetObject->GetUserData();
			if (NULL != bev)
			{
				bufferevent_write(bev, msg, nLen);

				mnSendMsgTotal++;
				return true;
			}
		}
	}

	return false;
}

bool LibeventNet::SendMsg(const char* msg, const size_t nLen, const std::list<UINT64>& fdList)
{
	std::list<UINT64>::const_iterator it = fdList.begin();
	for (; it != fdList.end(); ++it)
	{
		SendMsg(msg, nLen, *it);
	}

	return true;
}


bool LibeventNet::CloseNetObject(const UINT64 nSockIndex)
{
	std::map<UINT64, NetObject*>::iterator it = mmObject.find(nSockIndex);
	if (it != mmObject.end())
	{
		NetObject* pObject = it->second;

		pObject->SetNeedRemove(true);
		mvRemoveObject.push_back(nSockIndex);

		return true;
	}

	return false;
}

bool LibeventNet::Dismantle(NetObject* pObject)
{
	bool bNeedDismantle = false;

	int len = pObject->GetBuffLen();
	return bNeedDismantle;
}

bool LibeventNet::AddNetObject(const UINT64 nSockIndex, NetObject* pObject)
{
	//lock
	return mmObject.insert(std::map<UINT64, NetObject*>::value_type(nSockIndex, pObject)).second;
}

int LibeventNet::InitClientNet()
{
	std::string strIP = mstrIP;
	int nPort = mnPort;

	struct sockaddr_in addr;
	struct bufferevent* bev = NULL;

#if defined(__WINDOWS__)
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(nPort);

	if (evutil_inet_pton(AF_INET, strIP.c_str(), &addr.sin_addr) <= 0)
	{
		printf("inet_pton");
		return -1;
	}

	mxBase = event_base_new();
	if (mxBase == NULL)
	{
		printf("event_base_new ");
		return -1;
	}

	bev = bufferevent_socket_new(mxBase, -1, BEV_OPT_CLOSE_ON_FREE);
	if (bev == NULL)
	{
		printf("bufferevent_socket_new ");
		return -1;
	}

	int bRet = bufferevent_socket_connect(bev, (struct sockaddr*) & addr, sizeof(addr));
	if (0 != bRet)
	{
		//int nError = GetLastError();
		printf("bufferevent_socket_connect error");
		return -1;
	}

	UINT64 sockfd = bufferevent_getfd(bev);
	NetObject* pObject = new NetObject(this, sockfd, addr, bev);
	if (!AddNetObject(0, pObject))
	{
		return -1;
	}

	mbServer = false;

	bufferevent_setcb(bev, conn_readcb, conn_writecb, conn_eventcb, (void*)pObject);
	bufferevent_enable(bev, EV_READ | EV_WRITE | EV_CLOSED | EV_TIMEOUT | EV_PERSIST);

	event_set_log_callback(&LibeventNet::log_cb);

	bufferevent_set_max_single_read(bev, BUFFER_MAX_READ);
	bufferevent_set_max_single_write(bev, BUFFER_MAX_READ);

	int nSizeRead = (int)bufferevent_get_max_to_read(bev);
	int nSizeWrite = (int)bufferevent_get_max_to_write(bev);

	std::cout << "want to connect " << mstrIP << " SizeRead: " << nSizeRead << std::endl;
	std::cout << "SizeWrite: " << nSizeWrite << std::endl;

	return sockfd;
}

int LibeventNet::InitServerNet()
{
	int nPort = mnPort;

	struct sockaddr_in sin;

#if defined(__WINDOWS__)
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif
	//////////////////////////////////////////////////////////////////////////

	struct event_config* cfg = event_config_new();

#if defined(__WINDOWS__)

	mxBase = event_base_new_with_config(cfg);

#else

	//event_config_avoid_method(cfg, "epoll");
	if (event_config_set_flag(cfg, EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST) < 0)
	{

		return -1;
	}
	int nCpuCount = 8; //TODO
		if (event_config_set_num_cpus_hint(cfg, nCpuCount) < 0)
		{
			return -1;
		}

	mxBase = event_base_new_with_config(cfg); //event_base_new()

#endif
	event_config_free(cfg);

	//////////////////////////////////////////////////////////////////////////

	if (!mxBase)
	{
		fprintf(stderr, "Could not initialize libevent!\n");
		Final();

		return -1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(nPort);

	printf("server started with %d\n", nPort);

	listener = evconnlistener_new_bind(mxBase, listener_cb, (void*)this,
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
		(struct sockaddr*) & sin,
		sizeof(sin));

	if (!listener)
	{
		fprintf(stderr, "Could not create a listener!\n");
		Final();

		return -1;
	}

	mbServer = true;

	event_set_log_callback(&LibeventNet::log_cb);

	return mnMaxConnect;
}

bool LibeventNet::CloseSocketAll()
{
	std::map<UINT64, NetObject*>::iterator it = mmObject.begin();
	for (; it != mmObject.end(); ++it)
	{
		UINT64 nFD = it->first;
		mvRemoveObject.push_back(nFD);
	}

	ExecuteClose();

	mmObject.clear();

	return true;
}

NetObject* LibeventNet::GetNetObject(const UINT64 nSockIndex)
{
	auto it = mmObject.begin();
	for (; it != mmObject.end(); it++)
	{
		if (it->second->GetRealFD() == nSockIndex)
		{
			return it->second;
		}
	}

	return NULL;
}

void LibeventNet::CloseObject(const UINT64 nSockIndex)
{
	std::map<UINT64, NetObject*>::iterator it = mmObject.find(nSockIndex);
	if (it != mmObject.end())
	{
		NetObject* pObject = it->second;

		struct bufferevent* bev = (bufferevent*)pObject->GetUserData();

		bufferevent_free(bev);

		mmObject.erase(it);

		delete pObject;
		pObject = NULL;
	}
}

void LibeventNet::ExecuteClose()
{
	for (int i = 0; i < mvRemoveObject.size(); ++i)
	{
		UINT64 nSocketIndex = mvRemoveObject[i];
		CloseObject(nSocketIndex);
	}

	mvRemoveObject.clear();
}

void LibeventNet::log_cb(int severity, const char* msg)
{
	//LOG(FATAL) << "severity:" << severity << " " << msg;
}

bool LibeventNet::IsServer()
{
	return mbServer;
}

bool LibeventNet::Log(int severity, const char* msg)
{
	log_cb(severity, msg);
	return true;
}

bool LibeventNet::Init()
{
	return true;
}

void LibeventNet::ThreadLoop()
{
	cout << "Init Libevent thread:" << this->GetThreadId() << endl;
	while (1)
	{
		Execute();
		Sleep(50);
	}
}