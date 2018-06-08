/*************************************************************************
  > File Name: connection.h
  > Author: xyz
  > Mail: xiao13149920@foxmail.com 
  > Created Time: Fri 03 Feb 2017 12:00:28 PM CST
 ************************************************************************/
#ifndef SOLUTIONGATEWAY_CONNECTION_H_
#define SOLUTIONGATEWAY_CONNECTION_H_

#include "define.h"
#include "session_list.h"
#include <map>
namespace snetwork_xservice_solutiongateway {
class UVTcpServer {
	public:
		UVTcpServer();
		~UVTcpServer();

		UVTcpServer(const UVTcpServer& r) = delete;
		UVTcpServer& operator=(const UVTcpServer& r) = delete;
		UVTcpServer(UVTcpServer&& r) = delete;
		UVTcpServer& operator=(UVTcpServer&& r) = delete;

	public:
		int Run(void);
		void SetupMapRequest();

		const uv_loop_t* GetLoop(void) const {
			return m_loop;
		}
		uv_tcp_t GetTcpSever(void) const {
			return m_server;
		}
		struct sockaddr_in GetSockaddr(void) const {
			return m_addr;
		}
		static int GetSGID(void) {
			return m_iSGID;
		}

	protected:
		static void OnNewConnectionTcp(uv_stream_t* server, int status);
		static void EchoReadTcp(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf);
		static void OnClose(uv_handle_s* handle);
		int Init(void);
		static void AllocBuffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
		static void ParserJsonTcp(uv_stream_t* stream, const uv_buf_t* buf, ssize_t nread);
		static void ParserJsonTcp(uv_stream_t *stream);
	public:	
		static void WriteCB(uv_write_t *req, int status);
		static void WriteReqCB(uv_async_t* handle); 

	private:
		static char m_strSGID[10];
		static int m_iSGID;
		static char m_dbName[20];
		static uv_loop_t*	m_loop;
		static uv_tcp_t	m_server;
		struct sockaddr_in	m_addr;

	private:
		static void Login(char* body, uv_stream_t* stream);
		static void Logout(char* body, uv_stream_t* stream);
		static void ChangePassword(char* body, uv_stream_t* stream);
		static void ChangeBalance(char* body, uv_stream_t* stream);
		static void UpdateTrader(char* body, uv_stream_t* stream);
		static void GetTraders(char* body, uv_stream_t* stream);
		static void InsertUpdateManagerRights(char* body, uv_stream_t* stream);
		static void GetManagerAccess(char* body, uv_stream_t* stream);
		static void UserDeleteCheckReq(char* body, uv_stream_t* stream);
		static void UserDeleteReq(char* body, uv_stream_t* stream);
		static void CreateTrader(char* body, uv_stream_t* stream);
		static void GetOnlineTrader(char* body, uv_stream_t* stream);
		static void GetPositionsReq(char* body, uv_stream_t* stream);
		static void GetPositionsReqX(char* body, uv_stream_t* stream);
		static void GetTop5BoReq(char* body, uv_stream_t* stream);
		static void GetCustomBoReq(char* body, uv_stream_t* stream);
		static void GetSymbolBoReq(char* body, uv_stream_t* stream);
		static void GetTop5BoRemoveReq(char* body, uv_stream_t* stream);
		static void GetCustomBoRemoveReq(char* body, uv_stream_t* stream);
		static void GetSymbolBoRemoveReq(char* body, uv_stream_t* stream);
		static void AdminLoginReq(char* body, uv_stream_t* stream);
		static void AdminLogoutReq(char* body, uv_stream_t* stream);
		static void AdminChangePasswordReq(char* body, uv_stream_t* stream);
		static void CreateManagerReq(char* body, uv_stream_t* stream);
		static void UpdateManagerReq(char* body, uv_stream_t* stream);
		static void DeleteManagerReq(char* body, uv_stream_t* stream);
		static void UpdateManagerRightReq(char* body, uv_stream_t* stream);
		static void GetManagerRightReq(char* body, uv_stream_t* stream);
		static void GetManagersReq(char* body, uv_stream_t* stream);
		//for group
		static void CreateGroup(char* body, uv_stream_t* stream);
		static void UpdateGroup(char* body, uv_stream_t* stream);
		static void GetGroupDetailsById(char* body, uv_stream_t* stream);
		static void GetAllGroupForGateWay(char* body, uv_stream_t* stream);
		static void GetAllGroup(char* body, uv_stream_t* stream);
		static void DeleteGroup(char* body, uv_stream_t* stream);
		static void UpdateGroupSecurity(char* body, uv_stream_t* stream);
		static void UpdateGroupReport(char* body, uv_stream_t* stream);
	private:
		//typedef void (UVTcpServer::*StaticMemberFunPointer)(char*, uv_stream_t*);
		typedef void (*StaticMemberFunPointer)(char*, uv_stream_t*);
		typedef std::map<int, StaticMemberFunPointer> MapRequest;
		static MapRequest m_mapReq;
};

}

#endif

