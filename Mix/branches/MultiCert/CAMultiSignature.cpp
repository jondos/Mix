/*
 * CAMultiSignature.cpp
 *
 *  Created on: 17.07.2008
 *      Author: zenoxx
 */
#include "StdAfx.h"
#include "CABase64.hpp"
#include "CAUtil.hpp"
#include "xml/DOM_Output.hpp"
#include "CASignature.hpp"
#include "CAMultiSignature.hpp"

CAMultiSignature::CAMultiSignature()
{
	m_signatures = NULL;
	m_sigCount = 0;
}

CAMultiSignature::~CAMultiSignature()
{
	SIGNATURE* tmp;
	while(m_signatures != NULL)
	{
		//delete Signer and CertStore
		delete m_signatures->pSig;
		delete m_signatures->pCerts;
		m_signatures->pCerts = NULL;
		m_signatures->pSig = NULL;
		//store current pointer
		tmp = m_signatures;
		//go to next signature
		m_signatures = m_signatures->next;
		//delete currten signature
		delete tmp;
		tmp = NULL;
	}
}

SINT32 CAMultiSignature::addSignature(CASignature* a_signature, CACertStore* a_certs)
{
	if(a_signature == NULL || a_certs == NULL)
		return E_UNKNOWN;
	SIGNATURE* newSignature = new SIGNATURE;
	newSignature->pSig = a_signature->clone();
	newSignature->pCerts = a_certs;
	newSignature->next = m_signatures;
	m_signatures = newSignature;
	m_sigCount++;
	return E_SUCCESS;
}

SINT32 CAMultiSignature::signXML(UINT8* in,UINT32 inlen,UINT8* out,UINT32* outlen, bool appendCerts)
{
	if(in == NULL || inlen < 1 || out == NULL || outlen == NULL)
		return E_UNKNOWN;

	XERCES_CPP_NAMESPACE::DOMDocument* doc = parseDOMDocument(in, inlen);
	if(doc == NULL)
		return E_UNKNOWN;
	DOMElement* root = doc->getDocumentElement();
	if(signXML(root, appendCerts) != E_SUCCESS)
		return E_UNKNOWN;
	return DOM_Output::dumpToMem(root,out,outlen);
}

SINT32 CAMultiSignature::signXML(DOMNode* node, bool appendCerts)
{
	if(m_sigCount == 0)
	{
		CAMsg::printMsg(LOG_ERR, "Trying to sign a document with no signature-keys set!");
		return E_UNKNOWN;
	}
	//getting the Document an the Node to sign
	XERCES_CPP_NAMESPACE::DOMDocument* doc = NULL;
	DOMNode* elemRoot = NULL;
	if(node->getNodeType() == DOMNode::DOCUMENT_NODE)
	{
		doc = (XERCES_CPP_NAMESPACE::DOMDocument*)node;
		elemRoot = doc->getDocumentElement();
	}
	else
	{
		elemRoot = node;
		doc = node->getOwnerDocument();
	}

	//check if there are already Signatures and if so remove them first...
	DOMNode* tmpSignature = NULL;
	while(getDOMChildByName(elemRoot, "Signature", tmpSignature, false) == E_SUCCESS)
	{
		DOMNode* n = elemRoot->removeChild(tmpSignature);
		if (n != NULL)
		{
			n->release();
			n = NULL;
		}
	}
	//get SHA1-Digest
	UINT32 len = 0;
	UINT8* canonicalBuff = DOM_Output::makeCanonical(elemRoot, &len);
	if(canonicalBuff == NULL)
	{
		return E_UNKNOWN;
	}
	UINT8 dgst[SHA_DIGEST_LENGTH];
	SHA1(canonicalBuff, len, dgst);
	delete[] canonicalBuff;
	canonicalBuff = NULL;

	UINT8 digestValue[512];
	len = 512;
	if(CABase64::encode(dgst, SHA_DIGEST_LENGTH, digestValue, &len) != E_SUCCESS)
	{
		return E_UNKNOWN;
	}

	//append a signature for each SIGNATURE element we have
	SIGNATURE* currentSignature = m_signatures;
	UINT32 sigCount = 0;
	for(UINT32 i=0; i<m_sigCount; i++)
	{
		//Creating the Sig-InfoBlock....
		DOMElement* elemSignedInfo = createDOMElement(doc, "SignedInfo");
		DOMElement* elemCanonicalizationMethod = createDOMElement(doc, "CanonicalizationMethod");
		DOMElement* elemSignatureMethod = createDOMElement(doc, "SignatureMethod");
		DOMElement* elemReference = createDOMElement(doc, "Reference");
		DOMElement* elemDigestMethod = createDOMElement(doc, "DigestMethod");
		if(currentSignature->pSig->isDSA())	//DSA-Signature
		{
			setDOMElementAttribute(elemSignatureMethod, "Algorithm", (UINT8*)DSA_SHA1_REFERENCE);
		}
		else if(currentSignature->pSig->isRSA())
		{
			setDOMElementAttribute(elemSignatureMethod, "Algorithm", (UINT8*)RSA_SHA1_REFERENCE);
		}
		else if(currentSignature->pSig->isECDSA())
		{
			setDOMElementAttribute(elemSignatureMethod, "Algorithm", (UINT8*)ECDSA_SHA1_REFERENCE);
		}
		setDOMElementAttribute(elemDigestMethod, "Algorithm", (UINT8*)SHA1_REFERENCE);
		DOMElement* elemDigestValue = createDOMElement(doc, "DigestValue");
		setDOMElementValue(elemDigestValue, digestValue);

		elemSignedInfo->appendChild(elemCanonicalizationMethod);
		elemSignedInfo->appendChild(elemSignatureMethod);
		elemSignedInfo->appendChild(elemReference);
		elemReference->appendChild(elemDigestMethod);
		elemReference->appendChild(elemDigestValue);

		// Signing the SignInfo block....
		canonicalBuff = DOM_Output::makeCanonical(elemSignedInfo,&len);
		if(canonicalBuff==NULL)
		{
			return E_UNKNOWN;
		}

		UINT32 sigLen = currentSignature->pSig->getSignatureSize();
		UINT8 sigBuff[sigLen];
		SINT32 ret = currentSignature->pSig->sign(canonicalBuff, len, sigBuff, &sigLen);
		delete[] canonicalBuff;
		canonicalBuff = NULL;
		if(ret != E_SUCCESS)
		{
			continue;
		}
		UINT sigSize = 255;
		UINT8 sig[255];
		if(CABase64::encode(sigBuff, sigLen, sig, &sigSize) != E_SUCCESS)
		{
			continue;
		}

		//Makeing the whole Signature-Block....
		DOMElement* elemSignature = createDOMElement(doc,"Signature");
		DOMElement* elemSignatureValue = createDOMElement(doc,"SignatureValue");
		setDOMElementValue(elemSignatureValue,sig);
		elemSignature->appendChild(elemSignedInfo);
		elemSignature->appendChild(elemSignatureValue);

		//Append KeyInfo if neccassary
		if(appendCerts)
		{
			//Making KeyInfo-Block
			DOMElement* tmpElemCerts = NULL;
			if(currentSignature->pCerts->encode(tmpElemCerts, doc) == E_SUCCESS && tmpElemCerts != NULL)
			{
				DOMElement* elemKeyInfo = createDOMElement(doc, "KeyInfo");
				elemKeyInfo->appendChild(tmpElemCerts);
				elemSignature->appendChild(elemKeyInfo);
			}
		}
		elemRoot->appendChild(elemSignature);

		//goto next Signature
		currentSignature = m_signatures->next;
		sigCount++;
	}
	if(sigCount > 0)
	{
		//CAMsg::printMsg(LOG_DEBUG, "Appended %d Signature(s) to XML-Structure\n", sigCount);
		return E_SUCCESS;
	}
	return E_UNKNOWN;
}

SINT32 CAMultiSignature::verifyXML(const UINT8* const in,UINT32 inlen, CACertificate* a_cert)
{
	XERCES_CPP_NAMESPACE::DOMDocument* doc = parseDOMDocument(in,inlen);
	if(doc == NULL)
	{
		return E_UNKNOWN;
	}
	DOMElement* root = doc->getDocumentElement();
	if(root == NULL)
	{
		return E_UNKNOWN;
	}
	return CAMultiSignature::verifyXML(root, a_cert);
}

SINT32 CAMultiSignature::verifyXML(DOMNode* root, CACertificate* a_cert)
{
	CASignature* sigVerifier = new CASignature();
	if(sigVerifier->setVerifyKey(a_cert) != E_SUCCESS)
	{
		CAMsg::printMsg(LOG_ERR, "Failed to set verify Key!");
		return E_UNKNOWN;
	}
	UINT8* signatureMethod = sigVerifier->getSignatureMethod();

	DOMNodeList* signatureElements = getElementsByTagName((DOMElement*)root, "Signature");
	CAMsg::printMsg(LOG_DEBUG, "Found %d Signature(s) in XML-Structure\n", signatureElements->getLength());
	UINT8 dgst[255];
	UINT32 dgstlen=255;
	UINT8* out = NULL;
	UINT32 outlen;
	bool verified = false;
	//go through all appended Signatures an try to verify them with the given cert
	for(UINT32 i=0; i<signatureElements->getLength(); i++)
	{
		DOMNode* elemSignature = signatureElements->item(i);
		if(elemSignature == NULL)
			continue;
		DOMNode* elemSigInfo;
		getDOMChildByName(elemSignature, "SignedInfo", elemSigInfo);
		if(elemSigInfo == NULL)
			continue;;
		//check if SignatureMethod fits...
		DOMNode* elemSigMethod;
		getDOMChildByName(elemSigInfo, "SignatureMethod", elemSigMethod);
		UINT32 algLen = 255;
		UINT8* algorithm = new UINT8[255];
		getDOMElementAttribute(elemSigMethod, (const char*)"Algorithm", algorithm, &algLen);
		//if signatureMethod is set check if its equal
		if(signatureMethod != NULL &&
				strncmp((const char*)algorithm, (const char*)signatureMethod, algLen) != E_SUCCESS)
		{
			CAMsg::printMsg(LOG_DEBUG, "Did NOT find matching SignatureMethods: %s and %s!\n", signatureMethod, algorithm);
			continue;
		}
		DOMNode* elemSigValue;
		getDOMChildByName(elemSignature, "SignatureValue", elemSigValue);
		if(elemSigValue == NULL)
			continue;
		DOMNode* elemReference;
		getDOMChildByName(elemSigInfo, "Reference", elemReference);
		if(elemReference == NULL)
			continue;
		DOMNode* elemDigestValue;
		getDOMChildByName(elemReference, "DigestValue", elemDigestValue);
		if(elemDigestValue == NULL)
			continue;

		if(getDOMElementValue(elemDigestValue,dgst,&dgstlen)!=E_SUCCESS)
			continue;
		if(CABase64::decode(dgst,dgstlen,dgst,&dgstlen)!=E_SUCCESS)
			continue;
		if(dgstlen!=SHA_DIGEST_LENGTH)
			continue;
		UINT8 tmpSig[255];
		UINT32 tmpSiglen = 255;
		if(getDOMElementValue(elemSigValue,tmpSig,&tmpSiglen)!=E_SUCCESS)
			continue;
		if(CABase64::decode(tmpSig,tmpSiglen,tmpSig,&tmpSiglen)!=E_SUCCESS)
			continue;
		out = new UINT8[5000];
		outlen = 5000;
		if(DOM_Output::makeCanonical(elemSigInfo, out, &outlen) == E_SUCCESS)
		{
			if(sigVerifier->verify(out, outlen, tmpSig, tmpSiglen) == E_SUCCESS)
			{
				//CAMsg::printMsg(LOG_DEBUG, "Signature Verification successful!\n");
				verified = true;
				break;
			}
		}
		//CAMsg::printMsg(LOG_DEBUG, "Signature Verification not successful!\n");
		delete[] out;
		out = NULL;
		continue;
	}
	if(verified)
	{
		//the signature could be verified, now check digestValue
		//first remove Signature-nodes from root
		for(UINT32 i=0; i<signatureElements->getLength(); i++)
		{
			root->removeChild(signatureElements->item(i));
		}
		outlen = 5000;
		DOM_Output::makeCanonical(root, out, &outlen);
		//append Signature-nodes again
		for(UINT32 i=0; i<signatureElements->getLength(); i++)
		{
			root->appendChild(signatureElements->item(i));
		}

		UINT8 newDgst[SHA_DIGEST_LENGTH];
		SHA1(out, outlen, newDgst);
		delete[] out;
		out = NULL;
		for(int i=0; i<SHA_DIGEST_LENGTH; i++)
		{
			if(newDgst[i] != dgst[i])
			{
				CAMsg::printMsg(LOG_ERR, "Error checking XML-Signature DigestValue!\n");
				return E_UNKNOWN;
			}
		}
		return E_SUCCESS;
	}
	CAMsg::printMsg(LOG_ERR, "XML-Signature could not be verified!\n");
	return E_UNKNOWN;
}

/*SINT32 CAMultiSignature::encodeForXML(const UINT8* in, const UINT32* inlen, UINT8* out, UINT32* outlen)
{
	UINT8* rLen = in[3];
	UINT8* sLen = in[3 + rLen + 2];

	memset(out, 0, outlen);

	for(SINT32 i = (outlen/2) - rLen; i < rLen; i++)
	{
		out[i] = in[4 + i];
	}
	for(SINT32 j = outlen -sLen; j < sLen; j++)
	{
		out[j] = in[4 + rLen + 2];
	}
	return E_SUCCESS;
}

SINT32 CAMultiSignature::decodeForXML(UINT8* in, UINT32* inlen, UINT8** out, UINT32* outlen)
{
	UINT8 index = outlen + 4 + 2;
	if(in[0] < 0)
	{
		index++;
	}
	if(in[outlen/2] < 0)
	{
		index++;
	}
	out = UINT8[index];
	out[0] = 0x30;
	out[1] = index - 2;
	out[2] = 0x02;
	if(in[0] < 0)
	{
		ECDSA_
	}
	else
	{

	}
	for(SINT32 i = )
}*/

