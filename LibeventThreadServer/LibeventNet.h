#pragma once

#include <thread>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/thread.h>
#include <event2/event_compat.h>

#include <vector>
#include <list>
#include <map>
#include <string>

#include "ThreadBase.h"

#pragma pack(push, 1)
class LibeventNet;
class NetObject
{
public:
	NetObject(LibeventNet* pNet, UINT64 fd, sockaddr_in& addr, void* pBev)
	{
		mnLogicState = 0;
		mnGameID = 0;
		nFD = fd;
		bNeedRemove = false;

		m_pNet = pNet;

		m_pUserData = pBev;
		memset(&sin, 0, sizeof(sin));
		sin = addr;
	}

	virtual ~NetObject()
	{
	}

	int AddBuff(const char* str, size_t nLen)
	{
		mstrBuff.append(str, nLen);
		return (int)mstrBuff.length();
	}

	int CopyBuffTo(char* str, uint32_t nStart, uint32_t nLen)
	{
		if (nStart + nLen > mstrBuff.length())
		{
			return 0;
		}
		memcpy(str, mstrBuff.data() + nStart, nLen);
		return nLen;
	}

	int RemoveBuff(uint32_t nStart, uint32_t nLen)
	{
		if (nStart + nLen > mstrBuff.length())
		{
			return 0;
		}

		mstrBuff.erase(nStart, nLen);

		return (int)mstrBuff.length();
	}

	const char* GetBuff()
	{
		return mstrBuff.data();
	}

	int GetBuffLen() const
	{
		return (int)mstrBuff.length();
	}

	void* GetUserData()
	{
		return m_pUserData;
	}

	LibeventNet* GetNet()
	{
		return m_pNet;
	}


	bool NeedRemove()
	{
		return bNeedRemove;
	}

	void SetNeedRemove(bool b)
	{
		bNeedRemove = b;
	}

	UINT64 GetRealFD()
	{
		return nFD;
	}

private:
	sockaddr_in sin;
	void* m_pUserData;
	std::string mstrBuff;
	std::string mstrUserData;
	std::string mstrSecurityKey;

	int32_t mnLogicState;
	int32_t mnGameID;
	LibeventNet* m_pNet;
	//
	UINT64 nFD;
	bool bNeedRemove;
};

class LibeventNet : public ThreadBase
{
public:
    LibeventNet()
    {
        mxBase = NULL;
        listener = NULL;

        mstrIP = "";
        mnPort = 0;
        mbServer = false;
        mbWorking = false;

        mnSendMsgTotal = 0;
        mnReceiveMsgTotal = 0;

        mnBufferSize = 0;
    }
    virtual ~LibeventNet(){};

	bool Init();
	void ThreadLoop();

public:
     bool Execute();

     void InitNet(const char *strIP, const unsigned short nPort);
     int InitNet(const unsigned short nPort, const unsigned int nMaxClient);
     int InitNet(const char *strIP, const unsigned short nPort, const unsigned int nMaxClient);
     int ExpandBufferSize(const unsigned int size);

     bool KickAll();
     bool Final();

     bool CloseNetObject(const UINT64 nSockIndex);
	 bool AddNetObject(const UINT64 nSockIndex, NetObject* pObject);
	 NetObject* GetNetObject(const UINT64 nSockIndex);

	 bool IsServer();
	 bool Log(int severity, const char* msg);

	 bool SendMsgToAllClient(const char* msg, const size_t nLen);
	 bool SendMsg(const char* msg, const size_t nLen, const UINT64 nSockIndex);
	 bool SendMsg(const char* msg, const size_t nLen, const std::list<UINT64>& fdList);
public:
	void ExecuteClose();
	bool CloseSocketAll();

	bool Dismantle(NetObject* pObject);

	int InitClientNet();
	int InitServerNet();
	void CloseObject(const UINT64 nSockIndex);

    static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa, int socklen, void *user_data);
    static void conn_readcb(struct bufferevent *bev, void *user_data);
    static void conn_writecb(struct bufferevent *bev, void *user_data);
    static void conn_eventcb(struct bufferevent *bev, short events, void *user_data);
    static void log_cb(int severity, const char *msg);
    static void event_fatal_cb(int err);


private:
	std::map<UINT64, NetObject*> mmObject;
	std::vector<UINT64> mvRemoveObject;

    int mnMaxConnect;
    std::string mstrIP;
    int mnPort;
    bool mbServer;

    int mnBufferSize;

    bool mbWorking;
    bool mbTCPStream;

    int64_t mnSendMsgTotal;
    int64_t mnReceiveMsgTotal;

    struct event_base *mxBase;
    struct evconnlistener *listener;
};

#pragma pack(pop)
