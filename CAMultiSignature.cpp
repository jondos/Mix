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
		return E_UNKNOWN;

	UINT8 dgst[SHA_DIGEST_LENGTH];
	SHA1(canonicalBuff, len, dgst);
	delete[] canonicalBuff;
	canonicalBuff = NULL;

	UINT8 digestValue[512];
	len = 512;
	if(CABase64::encode(dgst, SHA_DIGEST_LENGTH, digestValue, &len) != E_SUCCESS)
		return E_UNKNOWN;
	digestValue[len] = 0;

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
				return E_UNKNOWN;

		UINT8 sigBuff[1024];
		if(currentSignature->pSig->getDSA() != NULL)	//DSA-Signature
		{
			setDOMElementAttribute(elemSignatureMethod, "Algorithm", (UINT8*)DSA_SHA1_REFERENCE);
			DSA_SIG* pdsaSig = NULL;
			SINT32 ret = currentSignature->pSig->sign(canonicalBuff, len, &pdsaSig);
			delete[] canonicalBuff;
			canonicalBuff = NULL;

			if(ret != E_SUCCESS)
			{
				DSA_SIG_free(pdsaSig);
				continue;
			}
			len = 1024;
			currentSignature->pSig->encodeRS(sigBuff, &len, pdsaSig);
			DSA_SIG_free(pdsaSig);
		}
		else if(currentSignature->pSig->getRSA() != NULL)	//RSA-Signature
		{
			setDOMElementAttribute(elemSignatureMethod, "Algorithm", (UINT8*)RSA_SHA1_REFERENCE);
			UINT32 sigLen = currentSignature->pSig->getSignatureSize();
			SINT32 ret = currentSignature->pSig->sign(canonicalBuff, len, sigBuff, &sigLen);
			delete[] canonicalBuff;
			canonicalBuff = NULL;

			if(ret != E_SUCCESS)
			{
				continue;
			}
			len = sigLen;
		}
		else
			continue;

		UINT sigSize = 255;
		UINT8 sig[255];
		if(CABase64::encode(sigBuff, len, sig, &sigSize) != E_SUCCESS)
			continue;
		sig[sigSize]=0;

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
			DOMElement* tmpElemCerts=NULL;
			if(currentSignature->pCerts->encode(tmpElemCerts, doc) == E_SUCCESS && tmpElemCerts != NULL)
			{
				DOMElement* elemKeyInfo=createDOMElement(doc,"KeyInfo");
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
		CAMsg::printMsg(LOG_DEBUG, "Appended %d Signatures to XML-Structure\n", sigCount);
		return E_SUCCESS;
	}
	return E_UNKNOWN;
}




