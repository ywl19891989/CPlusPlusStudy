#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdio.h>

#include "google/protobuf/text_format.h"

#include "GcpClient.h"
#include "TgcpConnectManager.h"
#include "lua_Network.h"

#include "cocos2d.h"
#include <signal.h>

#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
#include <ws2tcpip.h>
#define snprintf _snprintf
#elif CC_TARGET_PLATFORM == CC_PLATFORM_IOS
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#pragma mark MAC
#elif CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
#include <unistd.h>
#endif

#define DEVICE_ACTIVE_TIMEOUT 1

using namespace PVZ::Cmd;
using namespace std;
using namespace google::protobuf;
USING_NS_CC;

std::vector<std::string> m_vResponse;
pthread_mutex_t	m_pRecvMutex = PTHREAD_MUTEX_INITIALIZER;

std::vector<std::string> m_vRequest;
pthread_mutex_t m_pSendMutex = PTHREAD_MUTEX_INITIALIZER;

bool m_bStarted = false;
bool m_bConnected = false;
bool m_bNeedExit = false;
pthread_mutex_t	m_pFlagMutex = PTHREAD_MUTEX_INITIALIZER;

GcpClient* m_pGcpClient = new GcpClient;

void lockFlag(){
	pthread_mutex_lock(&m_pFlagMutex);
}

void unlockFlag(){
	pthread_mutex_unlock(&m_pFlagMutex);
}

void lockRecvQueue(){
	pthread_mutex_lock(&m_pRecvMutex);
}

void unlockRecvQueue(){
	pthread_mutex_unlock(&m_pRecvMutex);
}

void lockSendQueue(){
	pthread_mutex_lock(&m_pSendMutex);
}

void unlockSendQueue(){
	pthread_mutex_unlock(&m_pSendMutex);
}

void* SendMainLoop(void* _obj);
void* RecvMainLoop(void* _obj);

void cleanup(void*){
	lockFlag();
	if(m_bConnected){
		m_pGcpClient->finish();
		m_bConnected = false;
	}
	m_bStarted = false;
	unlockFlag();
}

void* RecvMainLoop(void* _obj)
{
	pthread_cleanup_push(cleanup, NULL);

	int iRet = 0;

	if((iRet = m_pGcpClient->init()) == 0 && (iRet = m_pGcpClient->connect()) == 0){

		lockFlag();
		m_bConnected = true;
		unlockFlag();

		pthread_t sendThread;
		pthread_create(&sendThread, NULL, SendMainLoop, NULL);
		pthread_detach(sendThread);

		std::string connectSuccess(CONNECT_SUCCESS);
		CmdCommon success;
		success.set_cmdname(connectSuccess.c_str(), connectSuccess.size());
		success.set_cmddata(connectSuccess.c_str(), connectSuccess.size());
		success.SerializeToString(&connectSuccess);
		lockRecvQueue();
		m_vResponse.push_back(connectSuccess);
		unlockRecvQueue();

		while(true) {

			bool isOK = true;
			lockFlag();
			isOK = m_bConnected && !m_bNeedExit;
			unlockFlag();
			if(!isOK){
				break;
			}

			char* dataPtr = NULL;
			int dataSize = 0;
			iRet = m_pGcpClient->recv(&dataPtr, &dataSize);

			if ( iRet == 0 && dataSize > 0) {
				std::string response(dataPtr, dataPtr + dataSize);
				lockRecvQueue();
				m_vResponse.push_back(response);
				unlockRecvQueue();
			} else if ( iRet != 0 ) {
				break;
			}
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
			Sleep(500);
#elif CC_TARGET_PLATFORM == CC_PLATFORM_IOS || CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
			usleep(500 * 1000);
#endif
		}
	}
	
	if(iRet != 0){
		std::string networkError(NETWORK_ERROR);
		CmdCommon cmdError;
		cmdError.set_cmdname(networkError.c_str(), networkError.size());
		cmdError.set_cmddata(networkError.c_str(), networkError.size());
		cmdError.set_cmdid(iRet);
		cmdError.SerializeToString(&networkError);
		lockRecvQueue();
		m_vResponse.push_back(networkError);
		unlockRecvQueue();
	}
	
	pthread_cleanup_pop(1);

	return NULL;
}

void* SendMainLoop(void* _obj)
{
	while(true)
	{
		bool isOK = true;
		lockFlag();
		isOK = m_bConnected && !m_bNeedExit;
		unlockFlag();
		if(!isOK){
			break;
		}

		static std::string request("");
		request.clear();
		lockSendQueue();
		if(!m_vRequest.empty()){
			request = m_vRequest[0];
			m_vRequest.erase(m_vRequest.begin());
		}
		unlockSendQueue();

		if(!request.empty()){
			m_pGcpClient->send(request.c_str(), request.size());
		}
	}
	return NULL;
}

TgcpConnectManager* TgcpConnectManager::s_pInstance = NULL;
TgcpConnectManager* TgcpConnectManager::Instance()
{
	if (NULL == s_pInstance)
	{
		s_pInstance = new TgcpConnectManager();
	}

	return s_pInstance;
}

void TgcpConnectManager::start(){
	
	bool needReturn = false;
	lockFlag();
	if(m_bStarted){
		needReturn = true;
	}
	unlockFlag();

	if(needReturn){
		return;
	}else{
		lockFlag();
		m_bStarted = true;
		pthread_t recvThread;
		pthread_create(&recvThread, NULL, RecvMainLoop, NULL);
		pthread_detach(recvThread);
		Director::getInstance()->getScheduler()->scheduleUpdateForTarget(this, 0, false);
		unlockFlag();
	}
}

void TgcpConnectManager::stop(){
	lockFlag();
	if(m_bStarted){
		m_bNeedExit = true;
	}
	unlockFlag();
}

void TgcpConnectManager::Destroy()
{
	if(s_pInstance != NULL){
		delete s_pInstance;
	}
}

void TgcpConnectManager::setUsrAccount(std::string account){
	m_pGcpClient->setUsrAccount(account);
}

void TgcpConnectManager::addEventListener(std::string packageName, void* pHandler, NetCallBackFun pCallbackFun){
	if ( pCallbackFun != NULL ) {
		NetCallBackPair pair;
		pair.m_pHandler = pHandler;
		pair.m_pCallbackFun = pCallbackFun;
		pair.m_sCmdType = packageName;
		m_vCallbackList.push_back(pair);
	}
}

void TgcpConnectManager::removeEventListener(std::string packageName, void* pHandler, NetCallBackFun pCallbackFun){
	std::vector<NetCallBackPair>::iterator it = m_vCallbackList.begin();
	while (it != m_vCallbackList.end())	{
		NetCallBackPair & p = *it;
		if(p.m_sCmdType == packageName && p.m_pHandler == pHandler && p.m_pCallbackFun == pCallbackFun){
			it = m_vCallbackList.erase(it);
		}else{
			it++;
		}
	}
}

bool TgcpConnectManager::SendPackedNetMessage(const PVZ::Cmd::CmdCommon &message)
{
	if (NULL == m_pGcpClient) {
		return false;
	} else {
		CheckConnection();
	}

	std::string sendData, cmdName;
	if (!message.SerializeToString(&sendData)) {
		return false; 
	}

	cmdName = message.cmdname();

	CCLOG("=======sending netmessage %s======", cmdName.c_str());
	
	lockSendQueue();
	m_vRequest.push_back(sendData);
	unlockSendQueue();

	std::string responseName = cmdName.substr(0, cmdName.size() - 2);
	responseName = responseName + "SC";

	CCLOG("send %s, wait %s", cmdName.c_str(), responseName.c_str());

	return true;
}

bool TgcpConnectManager::SendUnPackedNetMessage(const Message &message)
{
	PVZ::Cmd::CmdCommon pbCmd;
	const std::string& cmdName = message.GetTypeName();
	pbCmd.set_cmdname(cmdName.c_str(), cmdName.size());
	message.SerializeToString(pbCmd.mutable_cmddata());
	return SendPackedNetMessage(pbCmd);
}

void TgcpConnectManager::update(float delta)
{
	//listen if device resign active and become active back again

	m_fTimeElapsed += delta;
	if( m_fTimeElapsed > DEVICE_ACTIVE_TIMEOUT )
	{
		CheckConnection();
		m_fTimeElapsed = 0.0f;
	}
	static std::string response("");
	response.clear();
	lockRecvQueue();
	if(!m_vResponse.empty()){
		response = m_vResponse[0];
		m_vResponse.erase(m_vResponse.begin());
	}
	unlockRecvQueue();

	if(!response.empty()){
		CmdCommon commonCmd;
		if(commonCmd.ParseFromString(response)){
			if( commonCmd.cmdname() == CONNECT_SUCCESS){
				CCLOG("CONNECT_SUCCESS");
				handleMessage(CONNECT_SUCCESS, NULL, 0);
			}else if( commonCmd.cmdname() == NETWORK_ERROR){
				CCLOG("NETWORK_ERROR");
				handleMessage(NETWORK_ERROR, &commonCmd, 0);
			}else{
				parseCommand(commonCmd);

				std::vector<::google::protobuf::Message*>::iterator responseIt = m_vResponseList.begin();
				while(responseIt != m_vResponseList.end()) {
					::google::protobuf::Message* message = *responseIt;
					handleMessage(message->GetTypeName(), message, 0);
					responseIt = m_vResponseList.erase(responseIt);
					luaHandleMessage(NULL, message, 0);
					delete message;
				}
				m_vResponseList.clear();
			}
		}
	}
}

void TgcpConnectManager::handleMessage(std::string msgName, ::google::protobuf::Message* message, int errorCode){
	std::vector<NetCallBackPair>::iterator it = m_vCallbackList.begin();
	while (it != m_vCallbackList.end()) {
		NetCallBackPair& pair = (*it);
		if ( pair.m_sCmdType == msgName) {
			(*(pair.m_pCallbackFun))(pair.m_pHandler, message, errorCode);
			break;
		}
		++it;
	}
}

void TgcpConnectManager::parseCommand(const PVZ::Cmd::CmdCommon &stSvrCmd)
{
	if ( stSvrCmd.has_errorcode())
 	{
		if ( stSvrCmd.errorcode() < 0 )		//系统错误
		{
			stringstream str_logic_error;
			str_logic_error << "--------------- Sys errorCode " << stSvrCmd.errorcode() << "--------";
			CCLOGERROR( str_logic_error.str().c_str() );
			return;
		} else {
			stringstream str_logic_error;
			str_logic_error << "--------------- Logic errorCode " << stSvrCmd.errorcode() << "--------";
			CCLOGERROR( str_logic_error.str().c_str() );
		}
	}

	CCLOGERROR( "--------------------response messgaeName:-------------");
	CCLOGERROR( stSvrCmd.cmdname().c_str() );

	//构建返回消息
	Message *pCmd = NULL;
	const Descriptor *descriptor = DescriptorPool::generated_pool()->FindMessageTypeByName(stSvrCmd.cmdname());
	if (descriptor != NULL)
	{
		const Message *prototype = MessageFactory::generated_factory()->GetPrototype(descriptor);
		if (prototype != NULL)
		{
			pCmd = prototype->New();
			if (pCmd == NULL || !pCmd->ParseFromString(stSvrCmd.cmddata()))
			{
				CCLOGERROR( "--------- ParseError: parse cmddata error ----------");
				CCLOGERROR( stSvrCmd.cmdname().c_str() );
				CC_SAFE_DELETE(pCmd);
				return;
			}
		}
	}
	else
	{
		CCLOGERROR("---------------- Error! cannot find this meesage's descriptor! -----------------");
		return;
	}

	m_vResponseList.push_back(pCmd);
	CCLOG("response size %d", m_vResponseList.size());
}

TgcpConnectManager::TgcpConnectManager()
{
}

TgcpConnectManager::~TgcpConnectManager()
{
	stop();
}

bool TgcpConnectManager::GetMac(char* pBuffer)
{
	// TODO get platform mac addr
	strcpy(pBuffer, "MAC");

	return true;
}

void TgcpConnectManager::CheckConnection()
{

}
