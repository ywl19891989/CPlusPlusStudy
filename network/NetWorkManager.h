#ifndef _TGCP_CONNECT_MANAGER_H_
#define _TGCP_CONNECT_MANAGER_H_

#include <string>
#include <map>
#include <pthread.h>
#include "cocos2d.h"
#include "tgcpapi/clientconfdesc/client_conf_desc.h"

#include "command/CommandDefine.pb.h"


typedef void (*NetCallBackFun)(void* handler, ::google::protobuf::Message* response, uint32_t errorCode);

typedef struct _tagNetCallBackPair
{
	int		m_nErrorCode;
	std::string m_sCmdType;
	void*	m_pHandler;
	void*	m_pCmd;
	NetCallBackFun m_pCallbackFun;

	_tagNetCallBackPair()
	{
		m_nErrorCode = 0;
		m_sCmdType = "";
		m_pHandler = NULL;
		m_pCmd = NULL;
		m_pCallbackFun = NULL;
	}
}NetCallBackPair;

#define CONNECT_SUCCESS "ConnectSuccess"
#define NETWORK_ERROR "NetworkError"

bool iThread_Create(pthread_t * tid, void *(*start) (void *));
bool iThread_Destroy(pthread_t * tid);

class GcpClient;

class TgcpConnectManager : public cocos2d::Object
{
public:
	TgcpConnectManager();
	~TgcpConnectManager();

	void	start();
	void	stop();

	static TgcpConnectManager* Instance();
	static void Destroy();

	bool	Connect();
	bool	Connect(std::string szServer, int nPort);


	bool	SendUnPackedNetMessage(const ::google::protobuf::Message &message);
	bool 	SendPackedNetMessage(const PVZ::Cmd::CmdCommon &message);

	bool IsConnected();

	void	update(float delta);
	void	RecvMessage();

	static bool	GetMac(char* pBuffer);

	void addEventListener(std::string packageName, void* pHandler, NetCallBackFun pCallbackFun);
	void removeEventListener(std::string packageName, void* pHandler, NetCallBackFun pCallbackFun);

	void setUsrAccount(std::string account);

	void handleMessage(std::string msgName, ::google::protobuf::Message* message, int errorCode);

private:
	void parseCommand(const PVZ::Cmd::CmdCommon &message);

	void CheckConnection();

	void initLock();
	void lock();
	void unlock();
	void destroyLock();

	void initThread();
	void destroyThread();

private:
	static TgcpConnectManager* s_pInstance;


	std::map<uint32_t, std::string>	_mCmdMap;
	std::vector<NetCallBackPair> m_vCallbackList;
	std::vector<::google::protobuf::Message*> m_vResponseList;

	float m_fTimeElapsed;

};

#endif
