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
#ifndef __CASYMCIPHER__
#define __CASYMCIPHER__

#define KEY_SIZE 16

#include "CALockAble.hpp"

/** This class could be used for encryption/decryption of data (streams) with
  * AES using 128bit OFB mode. Because of the OFB mode technical encryption
	* and decrpytion are the same (depending on the kind of input). Therefore
	* there is only a general crypt() function.
	* This class has a 2-in-1 feature: Two independent IVs are available. Therefore
	* we have crypt1() and crypt2() depending on the used IV.
	*/
class CASymCipher
#ifndef ONLY_LOCAL_PROXY	
	:public CALockAble
#endif
	{
		public:
			CASymCipher()
				{
					m_bKeySet=false;
#ifdef INTEL_IPP_CRYPTO
					int size=0;
					ippsRijndael128GetSize(&size);
					m_keyAES1=(IppsRijndael128Spec*)new UINT8[size];
					m_keyAES2=(IppsRijndael128Spec*)new UINT8[size];
#else
					m_keyAES1=new AES_KEY;
					m_keyAES2=new AES_KEY;
#endif
					m_iv1=new UINT8[16];
					m_iv2=new UINT8[16];
				}

			~CASymCipher()
				{
#ifndef ONLY_LOCAL_PROXY
					waitForDestroy();
#endif
#ifdef INTEL_IPP_CRYPTO
					delete[] (UINT8*)m_keyAES1;
					delete[] (UINT8*)m_keyAES2;
#else
					delete m_keyAES1;
					delete m_keyAES2;
#endif
					m_keyAES1 = NULL;
					m_keyAES2 = NULL;
					delete[] m_iv1;
					m_iv1 = NULL;
					delete[] m_iv2;
					m_iv2 = NULL;
				}
			bool isKeyValid()
				{
					return m_bKeySet;
				}

			/** Sets the keys for crypt1() and crypt2() to the same key*/
			SINT32 setKey(const UINT8* key);	
			
			/** Sets the keys for crypt1() and crypt2() either to the same key (if keysize==KEY_SIZE) or to
			 * different values, if keysize==2* KEY_SIZE*/
			SINT32 setKeys(const UINT8* key,UINT32 keysize);	
			
			SINT32 setKey(const UINT8* key,bool bEncrypt);	

			/** Sets iv1 and iv2 to p_iv.
				* @param p_iv 16 random bytes used for new iv1 and iv2.
				* @retval E_SUCCESS
				*/
			SINT32 setIVs(const UINT8* p_iv)
				{
					memcpy(m_iv1,p_iv,16);
					memcpy(m_iv2,p_iv,16);
					return E_SUCCESS;
				}

			/** Sets iv2 to p_iv.
				* @param p_iv 16 random bytes used for new iv2.
				* @retval E_SUCCESS
				*/
			SINT32 setIV2(const UINT8* p_iv)
				{
					memcpy(m_iv2,p_iv,16);
					return E_SUCCESS;
				}

			SINT32 crypt1(const UINT8* in,UINT8* out,UINT32 len);
			SINT32 crypt2(const UINT8* in,UINT8* out,UINT32 len);
			SINT32 decrypt1CBCwithPKCS7(const UINT8* in,UINT8* out,UINT32* len);
			SINT32 encrypt1CBCwithPKCS7(const UINT8* in,UINT32 inlen,UINT8* out,UINT32* len);
	
			static SINT32 testSpeed();
		protected:

#ifdef INTEL_IPP_CRYPTO
			IppsRijndael128Spec* m_keyAES1;
			IppsRijndael128Spec* m_keyAES2;
#else
			AES_KEY* m_keyAES1;
			AES_KEY* m_keyAES2;
#endif

			UINT8* m_iv1;
			UINT8* m_iv2;
			bool m_bKeySet;
	};

#endif
