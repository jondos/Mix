/*
Copyright (c) The JAP-Team, JonDos GmbH

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation and/or
       other materials provided with the distribution.
    * Neither the name of the University of Technology Dresden, Germany, nor the name of
       the JonDos GmbH, nor the names of their contributors may be used to endorse or
       promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "CAPerformanceServer.hpp"

//////
#define DEBUG

#ifdef PERFORMANCE_SERVER

#include "CACmdLnOptions.hpp"
#include "CASocketAddrINet.hpp"

/**
 * @author Christian Banse
 * 
 * The server-side implementation of our performance monitoring
 * system. It creates a listening server socket which sends out 
 * random data based on a XML request.
 */

CAPerformanceServer* CAPerformanceServer::ms_pPerformanceServer = NULL;
extern CACmdLnOptions* pglobalOptions;

void CAPerformanceServer::init()
{
	if(ms_pPerformanceServer == NULL)
	{
		ms_pPerformanceServer = new CAPerformanceServer();
	}
}

void CAPerformanceServer::cleanup()
{
#ifdef DEBUG
	CAMsg::printMsg(LOG_DEBUG, 
			"CAPerformanceServer: doing cleanup\n");
#endif
	
	if(ms_pPerformanceServer != NULL)
	{
		delete ms_pPerformanceServer;
		ms_pPerformanceServer = NULL;
	}
}

CAPerformanceServer::CAPerformanceServer()
{
	m_pAcceptThread = NULL;
	m_pSocket = NULL;
	
	m_pSocket = new CASocket();
	
	if(initSocket() == E_SUCCESS)
	{
		m_pAcceptThread = new CAThread((UINT8*)"Performance Server Thread");
		m_pAcceptThread->setMainLoop(acceptRequests);
		m_pAcceptThread->start(this);
		
		m_pRequestHandler = new CAThreadPool(5, 2, true);
	}
	else
	{
		CAMsg::printMsg(LOG_ERR, 
				"CAPerformanceServer: error occured while creating server socket\n");
	}
}

CAPerformanceServer::~CAPerformanceServer()
{
	if(m_pAcceptThread != NULL)
	{
		if(m_pSocket != NULL)
		{
			if(!m_pSocket->isClosed())
			{
				m_pSocket->close();
			}
		}
		
		delete m_pAcceptThread;
		m_pAcceptThread = NULL;
		
		if(m_pRequestHandler != NULL)
		{
			delete m_pRequestHandler;
			m_pRequestHandler = NULL;
		}
	}
	
	if(m_pSocket != NULL)
	{
		delete m_pSocket;
		m_pSocket = NULL;
	}
}

SINT32 CAPerformanceServer::initSocket()
{
	SINT32 ret = E_UNKNOWN;
	CASocketAddrINet addr;
	
	if(m_pSocket == NULL)
	{
		m_pSocket = new CASocket();
	}
	
	if(!m_pSocket->isClosed())
	{
		CAMsg::printMsg(LOG_ERR, 
				"CAPerformanceServer: server socket already connected\n");
		return E_UNKNOWN;
	}
	
	ret = m_pSocket->create();
	if(ret != E_SUCCESS)
	{
		CAMsg::printMsg(LOG_ERR,
				"CAPerformanceServer: could not create server socket\n");
		return ret;
	}
	
	UINT8* host = PERFORMANCE_SERVER_HOST;
	UINT16 port = PERFORMANCE_SERVER_PORT;
	bool userdefined = false;
	
	if(pglobalOptions != NULL)
	{
		if(pglobalOptions->getPerformanceServerListenerHost() != NULL)
		{
			host = pglobalOptions->getPerformanceServerListenerHost();
			userdefined = true;
		}
		
		if(pglobalOptions->getPerformanceServerListenerPort() != 0xFFFF)
		{
			port = pglobalOptions->getPerformanceServerListenerPort();
			userdefined = true;
		}
	}
	
	ret = addr.setAddr(host, port);
	if(ret != E_SUCCESS)
	{
		if(ret == E_UNKNOWN_HOST)
		{
			CAMsg::printMsg(LOG_ERR,
					"CAPerformanceServer: invalid host specified: %s\n", host);
			
			if(userdefined)
			{
				host = PERFORMANCE_SERVER_HOST;
				CAMsg::printMsg(LOG_ERR,
						"CAPerformanceServer: trying %s\n", host);
				
				ret = addr.setAddr(host, port);
				
				if(ret != E_SUCCESS)
				{
					CAMsg::printMsg(LOG_ERR,
						"CAPerformanceServer: setting up listener interface on host %s:%d failed\n", host, port);
					return ret;
				}
			}
		}
		else
		{
			CAMsg::printMsg(LOG_ERR,
					"CAPerformanceServer: setting up listener interface on host %s:%d failed\n", host, port);
			return ret;
		}
	}
	
	ret = m_pSocket->listen(addr);
	if(ret != E_SUCCESS)
	{
		CAMsg::printMsg(LOG_ERR,
				"CAPerformanceServer: could not listen on %s:%d\n", host, port);
		return ret;
	}
	

	CAMsg::printMsg(LOG_DEBUG,
			"CAPerformanceServer: listening on %s:%d\n", host, port);
	
	return E_SUCCESS;
}

SINT32 CAPerformanceServer::sendDummyData(CASocket* pClient, UINT32 len)
{
	SINT32 ret = E_UNKNOWN;
	
	if(len > MAX_DUMMY_DATA_LENGTH)
		return E_UNKNOWN;

#ifdef DEBUG
	CAMsg::printMsg(LOG_DEBUG,
			"CAPerformanceServer: sending %d bytes of dummy data to client\n", len);
#endif	
	
	UINT8* buff = new UINT8[len];
	
	getRandom(buff, len);
	
	ret = pClient->sendFullyTimeOut((UINT8*)buff, len, 5000, 5000);
	delete buff;
	buff = NULL;
	
	return ret;
}

SINT32 CAPerformanceServer::handleRequest(perfrequest_t* request)
{
	char* line = new char[255];
	char* method = NULL;
	char* url = NULL;
	CASocket* pClient;
	
	UINT32 len = 0;
	
	if(request == NULL || request->pSocket == NULL)
	{
		return E_UNKNOWN;
	}
	
	pClient = request->pSocket;

	pClient->recieveLine((UINT8*)line, 255);
	
	method = strtok (line," ");
	
	if(method == NULL || strncmp(method, "POST", 3) != 0)
	{
#ifdef DEBUG
		CAMsg::printMsg(LOG_ERR,
				"CAPerformanceServer: 405 Method Not Allowed\n");

#endif
		sendHTTPResponseHeader(pClient, 405);
		return E_UNKNOWN;
	}
	
	url = strtok(NULL, " ");
	
	if(url == NULL || strncmp(url, "/generatedummydata", 18) != 0)
	{
#ifdef DEBUG		
		CAMsg::printMsg(LOG_ERR,
						"CAPerformanceServer: 404 Not Found\n");
#endif
		sendHTTPResponseHeader(pClient, 404);
		return E_UNKNOWN;
	}
	
	do
	{
		pClient->recieveLine((UINT8*)line, 255);
		if(strnicmp(line, "Content-Length: ", 16) == 0)
		{
			len = (UINT32) atol(line+16);
		}
	} while(strlen(line) > 0);
	delete line;
	
	if(len == 0)
	{
#ifdef DEBUG
		CAMsg::printMsg(LOG_DEBUG,
				"CAPerformanceServer: 400 Bad Request\n");
#endif
		sendHTTPResponseHeader(pClient, 400);
		return E_UNKNOWN;
	}
	else
	{
		UINT8* buff = new UINT8[len];
		memset(buff, 0, sizeof(UINT8)*len);
		pClient->receiveFullyT(buff, len, PERFORMANCE_SERVER_TIMEOUT);
		
		DOMDocument* doc = parseDOMDocument(buff, len);
		delete[] buff;
        buff = NULL;
        
        DOMElement* root = NULL;
        if(doc != NULL && (root = doc->getDocumentElement()) != NULL && equals(root->getNodeName(), "GenerateDummyDataRequest"))
        {
          	UINT32 length = 0;
           	getDOMElementAttribute(root, "dataLength", (SINT32*) &length);
           	
           	
           	if(length > MAX_DUMMY_DATA_LENGTH)
           	{
#ifdef DEBUG
        	CAMsg::printMsg(LOG_DEBUG,
        			"CAPerformanceServer: 400 Bad Request\n");
#endif
        	sendHTTPResponseHeader(pClient, 400);
        	return E_UNKNOWN;
           	}
           	
#ifdef DEBUG
           	CAMsg::printMsg(LOG_DEBUG,
           			"CAPerformanceServer: 200 OK\n");
#endif
           	sendHTTPResponseHeader(pClient, 200, length);
           	
           	sendDummyData(pClient, length);
            
            delete doc;
            doc = NULL;
            
            return E_SUCCESS;
        }
        else
        {
#ifdef DEBUG
        	CAMsg::printMsg(LOG_DEBUG,
        			"CAPerformanceServer: 400 Bad Request\n");
#endif
        	sendHTTPResponseHeader(pClient, 400);
        	return E_UNKNOWN;
        }
	}
}

SINT32 CAPerformanceServer::sendHTTPResponseHeader(CASocket* pClient, UINT16 code, UINT32 len)
{
	char* buff = new char[255];
	UINT32 ret = E_UNKNOWN;
	snprintf(buff, 255, "HTTP/1.1 %s\r\nContent-Length: %u\r\nConnection: close\r\n\r\n", getResponseText(code), len);
	
	ret = pClient->sendFullyTimeOut((UINT8*)buff, strlen(buff), PERFORMANCE_SERVER_TIMEOUT, PERFORMANCE_SERVER_TIMEOUT);
	delete buff;
	buff = NULL;
	
	return ret;
}

char* CAPerformanceServer::getResponseText(UINT16 code)
{
	switch(code)
	{
		case 200:
			return "200 OK";
			
		case 400:
			return "400 Bad Request";
			
		case 404:
			return "404 Not Found";
			
		case 405:
			return "405 Method Not Allowed";
			
		default:
			return "";
	}
}

THREAD_RETURN acceptRequests(void* param)
{
	CAPerformanceServer* pServer = (CAPerformanceServer*) param;
	
	if(pServer == NULL)
	{
		THREAD_RETURN_ERROR;
	}
	
	CASocket* pServerSocket = pServer->m_pSocket;
	
	while(true)
	{
		if(pServerSocket != NULL)
		{
			if(pServerSocket->isClosed())
			{
				CAMsg::printMsg(LOG_INFO,
						"CAPerformanceServer: socket closed. exiting listener thread\n");
				THREAD_RETURN_SUCCESS;
			}
		}
		else
		{
			CAMsg::printMsg(LOG_ERR,
					"CAPerformanceServer: socket disposed. exiting listener thread\n");
			THREAD_RETURN_ERROR;
		}
		
		CASocket* request = new CASocket;
		if(pServerSocket->accept(*request) == E_SUCCESS)
		{
			UINT8 peerIp;
			
			if(request->getPeerIP(&peerIp) == E_SUCCESS)
			{
				perfrequest_t* t = new perfrequest_t;
				t->ip = new char[16];
				strncpy(t->ip, inet_ntoa(*((in_addr*) &peerIp)), 15);
				
				t->pServer = pServer;
				t->pSocket = request;
				
#ifdef DEBUG
				CAMsg::printMsg(LOG_DEBUG, "CAPerformanceServer: accepting connection from %s\n", t->ip);
#endif
				pServer->m_pRequestHandler->addRequest(handleRequest, t);
			}
		}
	}
}

THREAD_RETURN handleRequest(void* param)
{
	SINT32 ret;
	perfrequest_t* request = (perfrequest_t*) param;
	
	if(request == NULL || request->pServer == NULL)
	{
		CAMsg::printMsg(LOG_INFO, "CAPerformanceServer: error occured while processing client request\n");
		THREAD_RETURN_ERROR;
	}
	
	ret = request->pServer->handleRequest(request);
	
	if(ret != E_SUCCESS)
	{
#ifdef DEBUG
		CAMsg::printMsg(LOG_DEBUG, "CAPerformanceServer: error while handling request from client %s (code: %d)\n", request->ip, ret);
#endif
	}
	
	request->pSocket->close();
	delete request->pSocket;
	delete request->ip;
	delete request;
	
	request = NULL;
	
	THREAD_RETURN_SUCCESS;
}
#endif /* PERFORMANCE_SERVER */
