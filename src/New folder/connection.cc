/*************************************************************************
  > File Name: connection.cc
  > Author: xyz
  > Mail: xiao13149920@foxmail.com 
  > Created Time: Fri 03 Feb 2017 12:00:04 PM CST
 ************************************************************************/
#include<unistd.h>
#include<string.h>
#include<string>
#include<stdlib.h>
#include<uuid/uuid.h> /* for uuid */
#include<arpa/inet.h> /*for ntohl*/
#include<rapidjson/document.h>
#include<rapidjson/writer.h>
#include<rapidjson/rapidjson.h>
#include<rapidjson/stringbuffer.h>
#include"connection.h"
#include"tnode_adapter.h"
#include"ThreadPool.h"
// #include"xdatetime.h"
#include"object_pool.h"
#include"safe_queue.h"
#include"solution_config.h"
#include <syscall.h>
#include "simplebuf.h"

using namespace snetwork_xservice_xflagger;
extern SafeList<CMTSessionListNode> g_solution_list;

extern ThreadPool* g_threadPool;
using AsyncTPool = ObjectPool<AsyncT, ASYNC_T_SIZE>;
extern AsyncTPool* g_asyncTPool;

extern SafeQueue<AsyncT*> g_asyncTQueue;
std::map<uv_stream_t*, SimpleBuf*> TcpConnBuf;

namespace snetwork_xservice_solutiongateway {

	bool VerifySession(rapidjson::Document &d, CMTSessionListNode &node) {
		rapidjson::Value::ConstMemberIterator it;
		if (((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)
			XINFO("field no exit or type error");
#endif

			return false;
		}
		const char *reqid = it->value.GetString();

		if (((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)
			XINFO("field no exit or type error");
#endif

			return false;
		}
		int managerid = it->value.GetInt();

		if (((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)
			XINFO("field no exit or type error");
#endif

			return false;
		}
		const char *session = it->value.GetString();

		node = CMTSessionListNode(session, UVTcpServer::GetSGID(), managerid);

		return true;
	}

/* Tcp Server begin */
	uv_loop_t *UVTcpServer::m_loop = nullptr;
	uv_tcp_t UVTcpServer::m_server;
	char UVTcpServer::m_strSGID[10];
	char UVTcpServer::m_dbName[20];
	int UVTcpServer::m_iSGID;
	UVTcpServer::MapRequest UVTcpServer::m_mapReq;

	UVTcpServer::UVTcpServer() {
		Init();
		SetupMapRequest();
	}

	UVTcpServer::~UVTcpServer() {
		uv_loop_close(m_loop);
		int r = uv_loop_close(m_loop);
		if (r == UV_EBUSY) {
			XERROR("uv_loop_close error %s\n", uv_strerror(r));    /* code */
		} else {
			free(m_loop);
		}
	}

	void UVTcpServer::SetupMapRequest() {
		m_mapReq.insert(std::make_pair(ETag::ELOGIN, &UVTcpServer::Login));
		m_mapReq.insert(std::make_pair(ETag::ELOGOUT, &UVTcpServer::Logout));
		m_mapReq.insert(std::make_pair(ETag::ECHANGEPASSWORD, &UVTcpServer::ChangePassword));
		m_mapReq.insert(std::make_pair(ETag::ECHANGEBALANCE, &UVTcpServer::ChangeBalance));
		m_mapReq.insert(std::make_pair(ETag::EUPDATETRADER, &UVTcpServer::UpdateTrader));
		m_mapReq.insert(std::make_pair(ETag::EGETTRADERS, &UVTcpServer::GetTraders));
		m_mapReq.insert(std::make_pair(ETag::EINSERTUPDATEMANAGERRIGHTS, &UVTcpServer::InsertUpdateManagerRights));
		m_mapReq.insert(std::make_pair(ETag::EGETMANAGERACCESS, &UVTcpServer::GetManagerAccess));
		m_mapReq.insert(std::make_pair(ETag::EUSERDELETECHECKREQ, &UVTcpServer::UserDeleteCheckReq));
		m_mapReq.insert(std::make_pair(ETag::EUSERDELETEREQ, &UVTcpServer::UserDeleteReq));
		m_mapReq.insert(std::make_pair(ETag::ECREATETRADER, &UVTcpServer::CreateTrader));
		m_mapReq.insert(std::make_pair(ETag::EGETONLINETRADER, &UVTcpServer::GetOnlineTrader));
		m_mapReq.insert(std::make_pair(ETag::EGETPOSITIONSREQ, &UVTcpServer::GetPositionsReq));
		m_mapReq.insert(std::make_pair(ETag::EGETPOSITIONSREQX, &UVTcpServer::GetPositionsReqX));
		m_mapReq.insert(std::make_pair(ETag::EGETTOP5BOREQ, &UVTcpServer::GetTop5BoReq));
		m_mapReq.insert(std::make_pair(ETag::EGETCUSTMBOREQ, &UVTcpServer::GetCustomBoReq));
		m_mapReq.insert(std::make_pair(ETag::EGETSYMBOLBOREQ, &UVTcpServer::GetSymbolBoReq));
		m_mapReq.insert(std::make_pair(ETag::EGETTOP5BOREMOVEREQ, &UVTcpServer::GetTop5BoRemoveReq));
		m_mapReq.insert(std::make_pair(ETag::EGETCUSTMBOREMOVEREQ, &UVTcpServer::GetCustomBoRemoveReq));
		m_mapReq.insert(std::make_pair(ETag::EGETSYMBOLBOREMOVEREQ, &UVTcpServer::GetSymbolBoRemoveReq));
		m_mapReq.insert(std::make_pair(ETag::EADMINLOGINREQ, &UVTcpServer::AdminLoginReq));
		m_mapReq.insert(std::make_pair(ETag::EADMINLOGOUTREQ, &UVTcpServer::AdminLogoutReq));
		m_mapReq.insert(std::make_pair(ETag::EADMINGCHANGEPASSWORDREQ, &UVTcpServer::AdminChangePasswordReq));
		m_mapReq.insert(std::make_pair(ETag::ECREATEMANAGERREQ, &UVTcpServer::CreateManagerReq));
		m_mapReq.insert(std::make_pair(ETag::EUPDATEMANAGERREQ, &UVTcpServer::UpdateManagerReq));
		m_mapReq.insert(std::make_pair(ETag::EDELETEMANAGERREQ, &UVTcpServer::DeleteManagerReq));
		m_mapReq.insert(std::make_pair(ETag::EUPDATEMANAGERRIGHTREQ, &UVTcpServer::UpdateManagerRightReq));
		m_mapReq.insert(std::make_pair(ETag::EGETMANAGERRIGHTREQ, &UVTcpServer::GetManagerRightReq));
		m_mapReq.insert(std::make_pair(ETag::EGETMANAGERSREQ, &UVTcpServer::GetManagersReq));
	}

	int UVTcpServer::Init(void) {
		m_loop = uv_default_loop();
		uv_tcp_init(m_loop, &m_server);
		SXConfig *sxconfig = dynamic_cast<SXConfig *>(SXFlagger::GetInstance()->GetConfig());
		memset(m_strSGID, 0, sizeof(m_strSGID));
		strncpy(m_strSGID, sxconfig->SolutionID().c_str(), 10);
		m_iSGID = atoi(m_strSGID);
		memset(m_dbName, 0, sizeof(m_dbName));
		strncpy(m_dbName, sxconfig->MySqlDBName().c_str(), 20);
		uv_ip4_addr("0.0.0.0", sxconfig->SolutionPort(), &m_addr);
		uv_tcp_nodelay((uv_tcp_t *) &m_server, 1);
		uv_tcp_bind(&m_server, (const struct sockaddr *) &m_addr, 0);

		uv_tcp_keepalive((uv_tcp_t *) &m_server, 1, 60 * 5);
		uv_tcp_simultaneous_accepts((uv_tcp_t *) &m_server, 1);
		int r = uv_listen((uv_stream_t *) &m_server, SOLUTIONGATEWAY_BACKLOG, OnNewConnectionTcp);
		if (r) {
			XERROR("Listen error %s\n", uv_strerror(r));

			return 1;
		}

		AsyncT **asyncT = new AsyncT *[ASYNC_T_SIZE];
		for (int i = 0; i < ASYNC_T_SIZE; ++i) {
			asyncT[i] = g_asyncTPool->GetObject();
			uv_async_init(m_loop, &asyncT[i]->data, WriteReqCB);
		}

		for (int i = 0; i < ASYNC_T_SIZE; ++i) {
			g_asyncTPool->GetInstance()->ReleaseObject(asyncT[i]);
		}
		delete[] asyncT;

		return 0;
	}


	void UVTcpServer::AllocBuffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
		buf->base = (char *) malloc(suggested_size);
		memset(buf->base, 0, suggested_size);
		buf->len = suggested_size;
	}

	void UVTcpServer::OnClose(uv_handle_s *handle) {
		//FREE(&handle);
		CMTSessionListNode node;
		node.m_stream = (uv_stream_t*)handle;
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			dst.m_stream = nullptr;
		});

		free(handle);
#if defined(DEBUG)
		XINFO("disconnection\n");
#endif
	}

	int UVTcpServer::Run(void) {
		return uv_run(m_loop, UV_RUN_DEFAULT);
	}

/* TCP connect begin */
	void UVTcpServer::OnNewConnectionTcp(uv_stream_t *server, int status) {
		if (status < 0) {
			XERROR("New connection error %s\n", uv_strerror(status));

			return;
		}

#if defined(DEBUG)
		XINFO("on_new_connection\n");
#endif
		uv_stream_t *stream = (uv_stream_t *) malloc(sizeof(uv_stream_t));
		if (stream == nullptr) {
			XERROR("malloc fail");

			return;
		}
		memset(stream, 0, sizeof(uv_stream_t));

		uv_tcp_init(server->loop, (uv_tcp_t *) stream);
		uv_tcp_nodelay((uv_tcp_t *) stream, 1);
		uv_tcp_keepalive((uv_tcp_t *) &stream, 1, 60 * 5);
		if (uv_accept(server, (uv_stream_t *) stream) == 0) {
			uv_read_start((uv_stream_t *) stream, UVTcpServer::AllocBuffer, UVTcpServer::EchoReadTcp);
			//TcpConnBuf.insert(std::make_pair(stream, new SimpleBuf));
#if defined(DEBUG)
			XINFO("uv_read_start end\n");
#endif
		} else {
			uv_close((uv_handle_t *) stream, UVTcpServer::OnClose);
#if defined(DEBUG)
			XINFO("uv_close end\n");
#endif
		}
#if defined(DEBUG)
		XINFO("uv_*** end\n");
#endif
	}

	void UVTcpServer::EchoReadTcp(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf) {
		if (nread < 0) {
			if (nread == UV_EOF) {
				XINFO("Read error: client close|%s\n", uv_err_name(nread));
			} else if (nread == UV_ECONNRESET) {
				XINFO("Read error: connect reset|%s\n", uv_err_name(nread));
			} else {
				XINFO("Read error %s\n", uv_err_name(nread));
			}
			free(buf->base);

			if (uv_is_active((uv_handle_t *) handle)) {
				uv_read_stop((uv_stream_t *) handle);
			}
			uv_close((uv_handle_t *) handle, UVTcpServer::OnClose);

			return;
		}

//		std::map<uv_stream_t*, SimpleBuf*>::iterator it;
//		if((it = TcpConnBuf.find(handle)) != TcpConnBuf.end()) {
//			it->second->WriteBuf(buf->base, buf->len);
//		}

		ParserJsonTcp((uv_stream_t *) handle, buf, nread);
		//ParserJsonTcp((uv_stream_t *)handle);
		free(buf->base);
	}

	void UVTcpServer::ParserJsonTcp(uv_stream_t *stream, const uv_buf_t *buf, ssize_t nread) {
		//printf("%lu, <%s>\n\n", nread, buf->base);
		if (nread <= sizeof(HeaderTag)) {
			return;
		}

		HeaderTag headerTag;
		headerTag.head = *(unsigned char *) buf->base;
		headerTag.tag = ntohs(*(unsigned short *) &buf->base[2]);
		headerTag.length = ntohl(*(unsigned int *) &buf->base[4]);
#if 0
        if (headerTag.head != 0x8F ||
            headerTag.length != (nread - sizeof(HeaderTag))) {
            return;
        }
#endif
		//char* body = NULL;
		//asprintf(&body, "%s", &buf->base[sizeof(HeaderTag)]);
		ssize_t hadRead = 0;
		while (true) {
			if (headerTag.head != 0x8F) {
				XERROR("Header invalid");

				return;
			}

			if (headerTag.length > nread) {
				XERROR("TCP error message");

				return;
			}

			//if nread-hadRead too large ,programm will exit!!!
			char *body = (char *) malloc(nread - hadRead);
			memcpy(body, &buf->base[8 + hadRead], headerTag.length);
			hadRead += headerTag.length + 8;

#if defined(DEBUG)
			XINFO("<Thread>:<%d>,tag=0x%02X,length=%d,body=%s", syscall(SYS_gettid), headerTag.tag, headerTag.length,
				  body);
#endif

			//to call the Request Function and deal with request
			MapRequest::iterator it;
			if ((it = m_mapReq.find(headerTag.tag)) != m_mapReq.end()) {
				// (this->*(it->second))(body, stream);
				//(UVTcpServer::*(it->second))(body, stream);
				(*(it->second))(body, stream);
			}

			std::this_thread::yield();

			if (hadRead == nread) {
				break;
			}

			headerTag.head = *(unsigned char *) &buf->base[hadRead];
			headerTag.tag = ntohs(*(unsigned short *) &buf->base[hadRead + 2]);
			headerTag.length = ntohl(*(unsigned int *) &buf->base[hadRead + 4]);

		}
	}

void UVTcpServer::ParserJsonTcp(uv_stream_t *stream) {
	std::map<uv_stream_t*, SimpleBuf*>::iterator it;
	if((it = TcpConnBuf.find(stream)) == TcpConnBuf.end()) {
		return;
	}

	int nread= it->second->GetBufLenght();
	if (nread <= sizeof(HeaderTag)) {
		return;
	}

	char* buf= it->second->GetBuf();
	HeaderTag headerTag;
	headerTag.head = *(unsigned char *) buf;
	headerTag.tag = ntohs(*(unsigned short *) &buf[2]);
	headerTag.length = ntohl(*(unsigned int *) &buf[4]);

	ssize_t hadRead = 0;
	while (true) {
		if (headerTag.head != 0x8F) {
			XERROR("Header invalid");

			return;
		}

		if ((headerTag.length + 8) > nread) {
			XERROR("TCP error message");

			return;
		}

		//if nread-hadRead too large ,programm will exit!!!
		char *body = (char *) malloc(nread - hadRead);
		memcpy(body, &buf[8 + hadRead], headerTag.length);
		hadRead += headerTag.length + 8;

#if defined(DEBUG)
		XINFO("<Thread>:<%d>,tag=0x%02X,length=%d,body=%s", syscall(SYS_gettid), headerTag.tag, headerTag.length,
			  body);
#endif

		//to call the Request Function and deal with request
		MapRequest::iterator it;
		if ((it = m_mapReq.find(headerTag.tag)) != m_mapReq.end()) {
			// (this->*(it->second))(body, stream);
			//(UVTcpServer::*(it->second))(body, stream);
			(*(it->second))(body, stream);
		}

		std::this_thread::yield();

		if (hadRead == nread) {
			break;
		}

		headerTag.head = *(unsigned char *) &buf[hadRead];
		headerTag.tag = ntohs(*(unsigned short *) &buf[hadRead + 2]);
		headerTag.length = ntohl(*(unsigned int *) &buf[hadRead + 4]);

	}
}

void UVTcpServer::WriteCB(uv_write_t *req, int status) {
	write_req_t* wr;
	wr = (write_req_t*) req;

	//int written = wr->buf.len;
	if (status != 0) {
		XERROR("async write", uv_strerror(status));
	}

	free(wr->buf.base);
	free(wr);
}

void UVTcpServer::Login(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
	#if defined(DEBUG)	
			XINFO("field no exit or type error");
	#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
	#if defined(DEBUG)	
			XINFO("field no exit or type error");
	#endif

			return;
		}
		int managerid = it->value.GetInt();

		/* add the peer name */
		struct sockaddr_in addr;
		int addrLen = sizeof(addr);
		uv_tcp_getpeername((uv_tcp_t*)stream, (struct sockaddr*)&addr, &addrLen);
		//uv_tcp_getsockname((uv_tcp_t*)stream, (struct sockaddr*)&addr, &addrLen);
		char ipAddrTmp[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr.sin_addr, ipAddrTmp, INET_ADDRSTRLEN);

		rapidjson::Value ipAddr(ipAddrTmp, d.GetAllocator());
		//d.AddMember("ipadress", ipAddr, d.GetAllocator());
		d.AddMember("addr", ipAddr, d.GetAllocator());

		/*add the management solution gateway id*/
		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		/* create uuid for login */
		uuid_t uuid;
		char session[33];
		uuid_generate_time_safe(uuid);
		DecToHex(session, (char*)&uuid);
		session[32] = 0;
		rapidjson::Value sessionid(session, d.GetAllocator());
		d.AddMember("session", sessionid, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		/*publish LOGIN.CM:  */
		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_LOGIN_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
													   buffer.GetString(),
													   buffer.GetLength()+1,
													   ETag::ELOGIN);

		CMTSessionListNode node(session, m_iSGID, managerid);
		node.m_stream = stream;
		node.m_clienttype = ECLIENTTYPE::ECMT;
		g_solution_list.PushFront(std::move(node));
	});
}

void UVTcpServer::Logout(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		// CMTSessionListNode node;
		// if(VerifySession(d, node)) {
		// 	g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
	#if defined(DEBUG)	
			XINFO("field no exit or type error");
	#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
	#if defined(DEBUG)	
			XINFO("field no exit or type error");
	#endif

			return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
	#if defined(DEBUG)	
			XINFO("field no exit or type error");
	#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			dst.m_status = ELogin::ELOGINEXIT;

			d.AddMember("sgid", UVTcpServer::GetSGID(), d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_LOGOUT_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::ELOGOUT);
		});
	});
}

void UVTcpServer::ChangePassword(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			// CMTSessionListNode node;
			// bool exist = VerifySession(d, node);
			// if(exist) {
			// g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_CHANGEPASSWORD_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::ECHANGEPASSWORD);
		});
	});
}

void UVTcpServer::ChangeBalance(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_CHANGEBALANCE_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::ECHANGEBALANCE);
		});
	});
}

void UVTcpServer::UpdateTrader(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_UPDATETRADER_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EUPDATETRADER);
		});
	});
}

void UVTcpServer::GetTraders(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		// node.m_stream = dst.m_stream;
		// g_broadcast_neworupdatetrader_list.PushFront(std::move(node));
		
		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETTRADERS_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EGETTRADERS);
		});
	});
}

void UVTcpServer::InsertUpdateManagerRights(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_INSERTUPDATEMANAGERRIGHTS_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EINSERTUPDATEMANAGERRIGHTS);
		});
	});
}

void UVTcpServer::GetManagerAccess(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETMANAGERACCESS_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EGETMANAGERACCESS);
		});
	});
}

void UVTcpServer::UserDeleteCheckReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_USERDELETECHECKREQ_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EUSERDELETECHECKREQ);
		});
	});
}

void UVTcpServer::UserDeleteReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_USERDELETEREQ_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EUSERDELETEREQ);
		});
	});
}

void UVTcpServer::CreateTrader(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_CREATETRADER_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::ECREATETRADER);
		});
	});
}

void UVTcpServer::GetOnlineTrader(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		
		// node.m_stream = dst.m_stream;
		// g_broadcast_onlinetrader_list.PushFront(std::move(node));

		//need to find a better way
		// CMTSessionListNode node(session, m_iSGID, managerid);
		// node.m_stream = dst.m_stream;
		// g_broadcast_logouttrader_list.PushFront(std::move(node));
		
		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETONLINETRADER_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_CMT_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EGETONLINETRADER);
		});
	});
}

//for RM/7
void UVTcpServer::GetPositionsReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {

		// node.m_stream = dst.m_stream;
		// g_broadcast_updatetrades_list.PushFront(std::move(node));

		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETPOSITIONSREQ_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_RM_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EGETPOSITIONSREQ);
		});
	});
}

void UVTcpServer::GetPositionsReqX(char* body, uv_stream_t* stream) {
		g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {

		// node.m_stream = dst.m_stream;
		// g_broadcast_updatetrades_list.PushFront(std::move(node));

		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETPOSITIONSREQX_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_RM_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EGETPOSITIONSREQX);
		});
	});
}

void UVTcpServer::GetTop5BoReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			// CMTSessionListNode* tmp = g_broadcast_top5bo_list.Find(node);
			// if(!tmp) {
				// node.m_stream = dst.m_stream;
				// g_broadcast_top5bo_list.PushFront(std::move(node));
			// }

			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETTOP5BOREQ_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_RM_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EGETTOP5BOREQ);
		});
	});
}

void UVTcpServer::GetCustomBoReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			// CMTSessionListNode* tmp = g_broadcast_custombo_list.Find(node);
			// if(!tmp) {
				// node.m_stream = dst.m_stream;
				// g_broadcast_custombo_list.PushFront(std::move(node));
			// }


			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETCUSTMBOREQ_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_RM_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EGETCUSTMBOREQ);
		});
	});
}

void UVTcpServer::GetSymbolBoReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			// CMTSessionListNode* tmp = g_broadcast_symbolbo_list.Find(node);
			// if(!tmp) {
				// node.m_stream = dst.m_stream;
				// g_broadcast_symbolbo_list.PushFront(std::move(node));
			// }

			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETSYMBOLBOREQ_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_RM_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EGETSYMBOLBOREQ);
		});
	});
}

void UVTcpServer::GetTop5BoRemoveReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		// g_broadcast_top5bo_list.Remove([&](CMTSessionListNode& r) {
		// 	if (r == node) {
		// 		 if m_status is not login success, remove node
		// 		return true;
		// 	}

		// 	return false;
		// });

		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETTOP5BOREMOVEREQ_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_RM_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EGETTOP5BOREMOVEREQ);
		});
	});
}		

void UVTcpServer::GetCustomBoRemoveReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		// g_broadcast_custombo_list.Remove([&](CMTSessionListNode& r) {
		// 	if (r == node) {
		// 		 //if m_status is not login success, remove node
		// 		return true;
		// 	}

		// 	return false;
		// });

		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETCUSTMBOREMOVEREQ_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_RM_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EGETCUSTMBOREMOVEREQ);
		});
	});
}	

void UVTcpServer::GetSymbolBoRemoveReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("managerid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int managerid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, managerid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		// g_broadcast_symbolbo_list.Remove([&](CMTSessionListNode& r) {
		// 	if (r == node) {
		// 		 //if m_status is not login success, remove node
		// 		return true;
		// 	}

		// 	return false;
		// });

		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETSYMBOLBOREMOVEREQ_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_RM_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EGETSYMBOLBOREMOVEREQ);
		});
	});
}

//for admin
void UVTcpServer::AdminLoginReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int adminid = it->value.GetInt();

//		/* add the peer name */
//		struct sockaddr_in addr;
//		int addrLen = sizeof(addr);
//		uv_tcp_getpeername((uv_tcp_t*)stream, (struct sockaddr*)&addr, &addrLen);
//		//uv_tcp_getsockname((uv_tcp_t*)stream, (struct sockaddr*)&addr, &addrLen);
//		char ipAddrTmp[INET_ADDRSTRLEN];
//		inet_ntop(AF_INET, &addr.sin_addr, ipAddrTmp, INET_ADDRSTRLEN);
//		rapidjson::Value ipAddr(ipAddrTmp, d.GetAllocator());
//		d.AddMember("ipadress", ipAddr, d.GetAllocator());
//		//d.AddMember("addr", ipAddr, d.GetAllocator());

		/*add the management solution gateway id*/
		d.AddMember("sgid", m_iSGID, d.GetAllocator());
		/* create uuid for login */
		uuid_t uuid;
		char session[33];
		uuid_generate_time_safe(uuid);
		DecToHex(session, (char*)&uuid);
		session[32] = 0;
		rapidjson::Value sessionid(session, d.GetAllocator());
		d.AddMember("session", sessionid, d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		/*publish LOGIN.CMT:  */
		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_ADMINLOGINREQ_NAME, 
												   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
												   buffer.GetString(),
												   buffer.GetLength()+1,
												   ETag::EADMINLOGINREQ);

		CMTSessionListNode node(session, m_iSGID, adminid);
		node.m_stream = stream;
		node.m_clienttype = ECLIENTTYPE::EADT;
		g_solution_list.PushFront(std::move(node));
	});
		
}

void UVTcpServer::AdminLogoutReq(char* body, uv_stream_t* stream) {
/*logout */
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
		XERROR("parser json error\n");
		free(body);

		return;
		}
		free(body);

		// CMTSessionListNode node;
		// if(VerifySession(d, node)) {
		// 	g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
		XINFO("field no exit or type error");
		#endif

		return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
		dst.m_status = ELogin::ELOGINEXIT;

		d.AddMember("sgid", UVTcpServer::GetSGID(), d.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_ADMINLOGOUTREQ_NAME, 
													   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
													   buffer.GetString(), 
													   buffer.GetLength()+1, 
													   ETag::EADMINLOGOUTREQ);
		});
	});
}

void UVTcpServer::AdminChangePasswordReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
			XINFO("field no exit or type error");
		#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
		#if defined(DEBUG)	
			XINFO("field no exit or type error");
		#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
		#if defined(DEBUG)	
			XINFO("field no exit or type error");
		#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			// CMTSessionListNode node;
			// bool exist = VerifySession(d, node);
			// if(exist) {
			// g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_ADMINGCHANGEPASSWORDREQ_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EADMINGCHANGEPASSWORDREQ);
		});
	});
}

void UVTcpServer::CreateManagerReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_CREATEMANAGERREQ_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::ECREATEMANAGERREQ);
		});
	});
}

void UVTcpServer::UpdateManagerReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_UPDATEMANAGERREQ_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EUPDATEMANAGERREQ);
		});
	});
}

void UVTcpServer::DeleteManagerReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_DELETEMANAGERREQ_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EDELETEMANAGERREQ);
		});
	});
}

void UVTcpServer::UpdateManagerRightReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_UPDATEMANAGERRIGHTREQ_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EUPDATEMANAGERRIGHTREQ);
		});
	});
}

void UVTcpServer::GetManagerRightReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETMANAGERRIGHTREQ_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EGETMANAGERRIGHTREQ);
		});
	});
}

void UVTcpServer::GetManagersReq(char* body, uv_stream_t* stream) {
	g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETMANAGERSREQ_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EGETMANAGERSREQ);
		});
	});
}

//for admin group
 void CreateGroup(char* body, uv_stream_t* stream) {
 		g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_CREATEGROUP_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::ECREATEGROUP);
		});
	});
 }
 
 void UpdateGroup(char* body, uv_stream_t* stream) {
 		g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_UPDATEGROUP_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EUPDATEGROUP);
		});
	});
 }
 
 void GetGroupDetailsById(char* body, uv_stream_t* stream) {
 		g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETGROUPDETAILSBYID_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EGETGROUPDETAILSBYID);
		});
	});
 }
 
 void GetAllGroupForGateWay(char* body, uv_stream_t* stream) {
 		g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETALLGROUPFORGATEWAY_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EGETALLGROUPFORGATEWAY);
		});
	});
 }
 
 void GetAllGroup(char* body, uv_stream_t* stream) {
 		g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_GETALLGROUP_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EGETALLGROUP);
		});
	});
 }
 
 void DeleteGroup(char* body, uv_stream_t* stream) {
 		g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_DELETEGROUP_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EDELETEGROUP);
		});
	});
 }
 
 void UpdateGroupSecurity(char* body, uv_stream_t* stream) {
 		g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_UPDATEGROUPSECURITY_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EUPDATEGROUPSECURITY);
		});
	});
 }
 
 void UpdateGroupReport(char* body, uv_stream_t* stream) {
 		g_threadPool->Enqueue([body, stream] {
		rapidjson::Document d;
		d.Parse(body);
		if (d.HasParseError()) {
			XERROR("parser json error\n");
			free(body);

			return;
		}
		free(body);

		rapidjson::Value::ConstMemberIterator it;
		if(((it = d.FindMember("reqid")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *reqid = it->value.GetString();

		if(((it = d.FindMember("adminid")) == d.MemberEnd()) || !it->value.IsInt()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		int adminid = it->value.GetInt();

		if(((it = d.FindMember("session")) == d.MemberEnd()) || !it->value.IsString()) {
#if defined(DEBUG)	
			XINFO("field no exit or type error");
#endif

			return;
		}
		const char *session = it->value.GetString();

		CMTSessionListNode node(session, m_iSGID, adminid);
		g_solution_list.Update(node, false, [&](CMTSessionListNode& dst) {
			d.AddMember("sgid", m_iSGID, d.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			d.Accept(writer);

			TNodeAministrator::GetInstance()->PublishGroup(SOLUTIONGATEWAY_PUBLISH_UPDATEGROUPREPORT_NAME, 
														   SOLUTIONGATEWAY_PUBLISH_ADT_EVENT,
														   buffer.GetString(), 
														   buffer.GetLength()+1, 
														   ETag::EUPDATEGROUPREPORT);
		});
	});
 }
 

void UVTcpServer::WriteReqCB(uv_async_t* handle) {
#if defined(DEBUG)
	fprintf(stderr, "<Thread>:<%d>++++++++++line:%d|%s\n", syscall(SYS_gettid),__LINE__, __FUNCTION__);
#endif
	
	AsyncT* asyncT = nullptr;
	while(g_asyncTQueue.Pop(asyncT)) {
		WriteReq<CMTSessionListNode, 2> *data = (WriteReq<CMTSessionListNode, 2> *) asyncT->data.data;
		uv_handle_s *handle = (uv_handle_s *) &asyncT->data;
		g_asyncTPool->ReleaseObject((AsyncT*)handle);

		if (data->node != nullptr) {
			int ret = g_solution_list.Update(*data->node, false, [&](CMTSessionListNode &dst) {
				uv_stream_t *stream = (uv_stream_t *) dst.m_stream;
				data->node->m_stream = stream;

				if(nullptr == dst.m_stream ||
					ELogin::ELOGINREJECT == dst.m_status ||
					0 == uv_is_active((uv_handle_t*)stream)) {
					XWARNING("Stream was disconnect, 'uv_is_active == 0'");
					dst.m_status = ELogin::ELOGINCLOSE;

					delete data->node;
					delete data;

					return;
				}

				if(nullptr == dst.m_stream ||
					ELogin::ELOGINREJECT == dst.m_status ||
					0 == uv_is_writable((uv_stream_t*)stream)) {
					XWARNING("Stream was disconnect, 'uv_is_writable == 0'");
					dst.m_status = ELogin::ELOGINUNWRITE;

					delete data->node;
					delete data;

					return;
				}
#if defined(DEBUG)
				fprintf(stderr, "%d|unicast|%s: dst.m_managerID: %d, dst.m_status: %d, dst.m_sessionID: %s, dst.m_stream->type: %d\n", __LINE__, 
					__FUNCTION__, dst.m_managerID, dst.m_status, dst.m_sessionID, dst.m_stream->type);
#endif
				uv_write((uv_write_t *) &data->req, (uv_stream_t *)stream, data->buf, 2,
						 [](uv_write_t *req, int status) {
							 WriteReq<CMTSessionListNode, 2> *wr = (WriteReq<CMTSessionListNode, 2> *) req;
							 int tag = ntohs(*(unsigned short *) &wr->buf[0].base[2]);
#if defined(DEBUG)
							 fprintf(stderr, "%d|unicast|%s+++: dst.m_managerID: %d, dst.m_status: %d, dst.m_sessionID: %s, dst.m_stream->type: %d\n\n", __LINE__, 
								__FUNCTION__, wr->node->m_managerID, wr->node->m_status, wr->node->m_sessionID, wr->node->m_stream->type);
#endif
							 if (status != 0) {
								 fprintf(stderr, "uv_writeCB|%s\n\n", uv_strerror(status));
								 g_solution_list.Update(*wr->node, false, [&](CMTSessionListNode &dst) {
									 uv_stream_t *stream = (uv_stream_t *) dst.m_stream;
									 dst.m_status = ELogin::ELOGINREJECT;
								 });
							 }

							 delete wr->node;
							 delete wr;
						 });
			});
			/* if connect was close, release memory, but delay until connection node remove from g_solution_list*/
			if(ret == 0) {
				delete data->node;
				delete data;
			}
		} else {
			int ret = g_solution_list.UpdateAll([&](CMTSessionListNode &dst) {
				uv_stream_t *stream = (uv_stream_t *) dst.m_stream;

				WriteReq<CMTSessionListNode, 2> *writeReq =  new WriteReq<CMTSessionListNode, 2>;
				CMTSessionListNode *node = new CMTSessionListNode(dst.m_sessionID, dst.m_sgID, dst.m_managerID);
				writeReq->header = data->header;
				writeReq->event = data->event;
				writeReq->buf[0].base = (char*)writeReq->header.get();
				writeReq->buf[0].len = data->buf[0].len;
				writeReq->buf[1].base = (char*)writeReq->event.get();
				writeReq->buf[1].len = data->buf[1].len;
				node->m_stream = dst.m_stream;
				//node->m_status = dst.m_status;
				writeReq->node = node;

				if(nullptr == dst.m_stream ||
					ELogin::ELOGINREJECT == dst.m_status ||
					0 == uv_is_active((uv_handle_t*)stream)) {
					XWARNING("Stream was disconnect, 'uv_is_active == 0'");
					dst.m_status = ELogin::ELOGINCLOSE;

					delete node;
					delete writeReq;

					return;
				}

				if(nullptr == dst.m_stream ||
					ELogin::ELOGINREJECT == dst.m_status ||
					0 == uv_is_writable((uv_stream_t*)stream)) {
					XWARNING("Stream was disconnect, 'uv_is_writable == 0'");
					dst.m_status = ELogin::ELOGINUNWRITE;

					delete node;
					delete writeReq;

					return;
				}	

#if defined(DEBUG)
				fprintf(stderr, "%d|broadcast|%s: dst.m_managerID: %d, dst.m_status: %d, dst.m_sessionID: %s, dst.m_stream->type: %d\n", __LINE__, 
					__FUNCTION__, dst.m_managerID, dst.m_status, dst.m_sessionID, dst.m_stream->type);
#endif				
				uv_write((uv_write_t *) &writeReq->req, (uv_stream_t *) stream, writeReq->buf, 2,
						 [](uv_write_t *req, int status) {
							 WriteReq<CMTSessionListNode, 2> *wr = (WriteReq<CMTSessionListNode, 2> *) req;
							 int tag = ntohs(*(unsigned short *) &wr->buf[0].base[2]);
#if defined(DEBUG)
							 fprintf(stderr, "%d|broadcast|%s+++: dst.m_managerID: %d, dst.m_status: %d, dst.m_sessionID: %s, dst.m_stream->type: %d\n\n", __LINE__, 
								__FUNCTION__, wr->node->m_managerID, wr->node->m_status, wr->node->m_sessionID, wr->node->m_stream->type);
#endif
							 if (status != 0) {
								 fprintf(stderr, "uv_writeCB|%s\n\n", uv_strerror(status));
								 g_solution_list.Update(*wr->node, false, [&](CMTSessionListNode &dst) {
									 uv_stream_t *stream = (uv_stream_t *) dst.m_stream;
									 dst.m_status = ELogin::ELOGINREJECT;
								 });
							 }

							 delete wr->node;
							 delete wr;
						 });
			});

			delete data;
		}
	}
}
/* TCP connect end */
}
