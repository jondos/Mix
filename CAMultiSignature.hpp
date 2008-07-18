/*
 * CAMultiSignature.hpp
 *
 *  Created on: 17.07.2008
 *      Author: zenoxx
 */

#ifndef CAMULTISIGNATURE_HPP_
#define CAMULTISIGNATURE_HPP_

#include "CACertStore.hpp"
#include "CACertificate.hpp"

struct __t_signature
{
	CASignature* pSig;
	CACertStore* pCerts;
	struct __t_signature* next;
};
typedef struct __t_signature SIGNATURE;

class CAMultiSignature
{
	public:
		CAMultiSignature();
		virtual ~CAMultiSignature();
		SINT32 addSignature(CASignature* a_signature, CACertStore* a_certs);
		SINT32 signXML(DOMNode* a_node, bool appendCerts);
		SINT32 signXML(UINT8* in,UINT32 inlen,UINT8* out,UINT32* outlen, bool appendCerts);
		static SINT32 verifyXML(DOMNode* a_node, CACertificate* a_cert);
		UINT32 getSignatureCount(){ return m_sigCount; }
	private:
		SIGNATURE* m_signatures;
		UINT32 m_sigCount;
};

#endif /* CAMULTISIGNATURE_HPP_ */
