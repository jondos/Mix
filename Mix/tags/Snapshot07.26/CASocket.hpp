#ifndef __CASOCKET__
#define __CASOCKET__
/*
Copyright (c) 2000, The JAP-Team 
All rights reserved.
Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

	- Redistributions of source code must retain the above copyright notice, 
	  this list of conditions and the following disclaimer.

	- Redistributions in binary form must reproduce the above copyright notice, 
	  this list of conditions and the following disclaimer in the documentation and/or 
		other materials provided with the distribution.

	- Neither the name of the University of Technology Dresden, Germany nor the names of its contributors 
	  may be used to endorse or promote products derived from this software without specific 
		prior written permission. 

	
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS 
OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
*/
#include "CASocketAddr.hpp"
#include "CAClientSocket.hpp"
#include "CAMutex.hpp"

class CASocket:public CAClientSocket
	{
		public:
		CASocket(bool bIsReserved=false);
			~CASocket(){close();}

			SINT32 create();						
		    SINT32 create(bool a_bShowTypicalError);
			SINT32 create(int type);

			SINT32 listen(const CASocketAddr& psa);
			SINT32 listen(UINT16 port);
			SINT32 accept(CASocket &s);
			
			SINT32 connect(const CASocketAddr& psa)
			{
				return connect(psa,1,0);
			}
			SINT32 connect(const CASocketAddr& psa,UINT32 retry,UINT32 sWaitTime);
			SINT32 connect(const CASocketAddr& psa,UINT32 msTimeOut);
			
			SINT32 close();
/* it seems that this function is not used:
			SINT32 close(UINT32 mode);*/
			SINT32 send(const UINT8* buff,UINT32 len);
			SINT32 sendFully(const UINT8* buff,UINT32 len);
			SINT32 sendFullyTimeOut(const UINT8* buff,UINT32 len, UINT32 msTimeOut, UINT32 msTimeOutSingleSend);
			SINT32 sendTimeOut(const UINT8* buff,UINT32 len,UINT32 msTimeOut);
			SINT32 receive(UINT8* buff,UINT32 len);
			SINT32 receiveFullyT(UINT8* buff,UINT32 len,UINT32 msTimeOut);
			SINT32 receiveLine(UINT8* line, UINT32 maxLen, UINT32 msTimeOut);
			/** Returns the number of the Socket used. Which will be always the same number,
				* even after close(), until the Socket
				* is recreated using create()
				* @return number of the associated socket
			**/
			operator SOCKET(){return m_Socket;}
			SINT32 getLocalPort();
			SINT32 getPeerIP(UINT8 ip[4]);
			SINT32 setReuseAddr(bool b);
			//SINT32 setRecvLowWat(UINT32 r);
			//SINT32 setSendLowWat(UINT32 r);
			//SINT32 getSendLowWat();
			SINT32 setSendTimeOut(UINT32 msTimeOut);
			SINT32 getSendTimeOut();
			SINT32 setRecvBuff(UINT32 r);
			SINT32 getRecvBuff();
			SINT32 setSendBuff(SINT32 r);
			SINT32 getSendBuff();
			SINT32 setKeepAlive(bool b);
			SINT32 setKeepAlive(UINT32 sec);
			SINT32 setNonBlocking(bool b);
			SINT32 getNonBlocking(bool* b);
			
			/** Sets the max number of allowed "normal" sockets.
				* @retval E_SUCCESS if call was successful
				* @retval E_UNKNOWN otherwise
				*/
			static SINT32 setMaxNormalSockets(UINT32 u)
				{
				m_u32MaxNormalSockets=u;
				return E_SUCCESS;
				}
			
			/** Tries to find out how many socket we can open by open as many socket as possible witthout errors.
				* If we can open more than 10.000 sockets we stop the test and return 10000.
				*@retval max numbers of sockets we can have open at the same time
				*@retval E_UNKNOWN in case of some unexpected error
			*/
			static SINT32 getMaxOpenSockets();
			bool isClosed()
			{
				return m_bSocketIsClosed;
			}
			SINT32 getLocalIP(UINT32* r_Ip);
			
		protected:
	///check	
/*			CASocket* getSocket()
			{
				return this;
			}
*/		
///end check	
			bool m_bSocketIsClosed; //this is a flag, which shows, if the m_Socket is valid
													//we should not set m_Socket to -1 or so after close,
													//because the Socket value ist needed sometimes even after close!!!
													// (because it is used as a Key in lookups for instance as a HashValue etc.)

			SOCKET m_Socket;
		private:			
			SINT32 create(int type, bool a_bShowTypicalError);
		
			CAMutex m_csClose;
			///The following two variables are use to realise "reserved" sockets. The rational behind is to ensure
			///that we could allway crate "reserved" socket why we may fail to create normal sockets because of to many open files related restrictions
			static UINT32 m_u32NormalSocketsOpen; //how many "normal" sockets are open
			static UINT32 m_u32MaxNormalSockets; //how many "normal" sockets are allowed at max
			bool m_bIsReservedSocket; ///Normal or reserved socket?
	};
#endif