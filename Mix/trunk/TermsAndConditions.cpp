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

#include "TermsAndConditions.hpp"
#include "CAMsg.hpp"
#include "CACmdLnOptions.hpp"
#include "CAUtil.hpp"

/**
 * IMPORTANT NOTE:
 * all methods does NOT incorporate locking over the translation store and thus
 * are NOT THREAD-SAFE. If a caller obtains a reference to a translation entry
 * he must lock it as long as he works with it by using the lock object
 * provided by "getSynchLock".
 */

void cleanupTnCMixAnswer(termsAndConditionMixAnswer_t *answer)
{
	if( (answer != NULL) )
	{
		if((answer->xmlAnswer != NULL))
		{
			answer->xmlAnswer->release();
			answer->xmlAnswer = NULL;
		}
		answer->exchangeFinished = false;
	}
}

void cleanupTnCTranslation(termsAndConditionsTranslation_t *tnCTranslation)
{
	if(tnCTranslation != NULL)
	{
		tnCTranslation->tnc_id = NULL;
		tnCTranslation->tnc_template = NULL;
		if(tnCTranslation->tnc_customized != NULL)
		{
			tnCTranslation->tnc_customized->release();
		}
		tnCTranslation->tnc_customized = NULL;
		delete [] tnCTranslation->tnc_locale;
		tnCTranslation->tnc_locale = NULL;
	}
}

//#ifdef DEBUG
void printTranslation(termsAndConditionsTranslation_t *tnCTranslation)
{
	if(tnCTranslation != NULL)
	{
		CAMsg::printMsg(LOG_DEBUG, "Translation [%s] of T&C %s found.\n",tnCTranslation->tnc_locale, tnCTranslation->tnc_id);

		UINT32 debuglen = 3000;
		UINT8 debugout[3000];
		DOM_Output::dumpToMem(tnCTranslation->tnc_customized,debugout,&debuglen);
		debugout[debuglen] = 0;
		CAMsg::printMsg(LOG_DEBUG, "the Customized T&C part looks like this: %s \n",debugout);

		UINT32 debuglen2 = 50000;
		UINT8 debugout2[50000];
		DOM_Output::dumpToMem(tnCTranslation->tnc_template,debugout2,&debuglen2);
		debugout2[debuglen2] = 0;
		puts((char *)debugout2);
	}
}
//#endif

/**
 * Constructor with the id (Operator SKI) and the number of translations
 */
TermsAndConditions::TermsAndConditions(UINT8* id, UINT32 nrOfTranslations)
{
	custmoziedSectionsOwner = createDOMDocument();
	/* The id of the Terms & Conditions is the operator ski. */
	if(id != NULL)
	{
		size_t idlen = strlen((char *) id);
		tnc_id = new UINT8[idlen+1];
		memset(tnc_id, 0, (idlen+1));
		memcpy(tnc_id, id, idlen);
	}
	else
	{
		tnc_id = NULL;
	}

	translations = (nrOfTranslations == 0) ? 1 : nrOfTranslations;
	currentTranslationIndex = 0;
	allTranslations = new termsAndConditionsTranslation_t *[translations];
	for (UINT32 i = 0; i < translations; i++)
	{
		allTranslations[i] = NULL;
	}
	synchLock = new CAMutex();
}

TermsAndConditions::~TermsAndConditions()
{
	delete synchLock;
	synchLock = NULL;
	for (UINT32 i = 0; i < translations; i++)
	{
		if(allTranslations[i] != NULL)
		{
			cleanupTnCTranslation(allTranslations[i]);
			delete allTranslations[i];
			allTranslations[i] = NULL;
		}
	}
	delete [] allTranslations;
	allTranslations = NULL;
	delete [] tnc_id;
	tnc_id = NULL;
	translations =  0;
	currentTranslationIndex = 0;
	custmoziedSectionsOwner->release();
	custmoziedSectionsOwner = NULL;
}

/**
 * returns the specific TermsAndconditions translation for the the language with the
 * specified language code including the template AND the customized sections.
 * If no translation exists for the specified language code NULL is returned.
 */
termsAndConditionsTranslation_t *TermsAndConditions::getTranslation(const UINT8 *locale)
{
	if(locale == NULL)
	{
		return NULL;
	}
	termsAndConditionsTranslation_t *foundEntry = NULL;
	for (UINT32 i = 0; i < translations; i++)
	{
		if(allTranslations[i] != NULL)
		{
			if( strncasecmp((const char *) locale, (char *) allTranslations[i]->tnc_locale, 2) == 0 )
			{
				foundEntry = allTranslations[i];
				break;
			}
		}
	}
	return foundEntry;
}

/**
 * returns only the template of the translation specified by the language code
 * or NULL if no such translation exist.
 */
XERCES_CPP_NAMESPACE::DOMDocument *TermsAndConditions::getTranslationTemplate(const UINT8 *locale)
{
	termsAndConditionsTranslation_t *foundEntry = getTranslation(locale);
	if(foundEntry != NULL)
	{
		return foundEntry->tnc_template;
	}
	return NULL;
}

/**
 * returns only the customized sections of the translation specified by the language code
 * or NULL if no such translation exist.
 */
DOMNode *TermsAndConditions::getTranslationCustomizedSections(const UINT8 *locale)
{
	termsAndConditionsTranslation_t *foundEntry = getTranslation(locale);
	if(foundEntry != NULL)
	{
		return foundEntry->tnc_customized;
	}
	return NULL;
}

/**
 * add a language specific terms and Conditions document, which can be
 * retrieved by *getTermsAndConditionsDoc with the language code
 */
void TermsAndConditions::addTranslation(const UINT8* locale, DOMNode *tnc_customized, XERCES_CPP_NAMESPACE::DOMDocument *tnc_template)
{
	if(locale == NULL)
	{
		return;
	}
	termsAndConditionsTranslation_t *newEntry = getTranslation(locale);
	if(newEntry == NULL)
	{
		newEntry = new termsAndConditionsTranslation_t;
		//import the customized sections to the internal T & C document to ensure it is not
		//released by it's former owner document.
		newEntry->tnc_customized = custmoziedSectionsOwner->importNode(tnc_customized->cloneNode(true), true);
		newEntry->tnc_template = tnc_template;
		newEntry->tnc_id = tnc_id;
		newEntry->tnc_locale = new UINT8[TMP_LOCALE_SIZE];
		memset(newEntry->tnc_locale, 0, TMP_LOCALE_SIZE);
		memcpy(newEntry->tnc_locale, locale, TMP_LOCALE_SIZE);

		if(currentTranslationIndex == translations)
		{
			//allocate new storage space.
			termsAndConditionsTranslation_t **newAllocatedSpace = new termsAndConditionsTranslation_t *[translations+RESIZE_STORE];
			memset(newAllocatedSpace, 0, (sizeof(termsAndConditionsTranslation_t *)*(translations+RESIZE_STORE)) );
			memcpy(newAllocatedSpace, allTranslations,
					(sizeof(termsAndConditionsTranslation_t *)*translations) );
			delete [] allTranslations;
			allTranslations = newAllocatedSpace;
			translations += RESIZE_STORE;
		}
		allTranslations[currentTranslationIndex] = newEntry;
		CAMsg::printMsg(LOG_DEBUG,"Adding translation [%s] for t&c %s to index %u\n", newEntry->tnc_locale, newEntry->tnc_id, currentTranslationIndex);
		setIndexToNextEmptySlot();
	}
	else
	{
		newEntry->tnc_customized->release();
		//same as above: avoid release by the former owner document of this node.
		newEntry->tnc_customized = custmoziedSectionsOwner->importNode(tnc_customized->cloneNode(true), true);
		newEntry->tnc_template = tnc_template;
	}
}

/**
 * removes the specific Terms & Conditions translation for the the language with the
 * specified language code including the template AND the customized sections.
 */
void TermsAndConditions::removeTranslation(const UINT8 *locale)
{
	for (UINT32 i = 0; i < translations; i++)
	{
		if(allTranslations[i] != NULL)
		{
			if( strncasecmp((const char *) locale, (char *) allTranslations[i]->tnc_locale, 2) == 0 )
			{
				cleanupTnCTranslation(allTranslations[i]);
				delete allTranslations[i];
				allTranslations[i] = NULL;
				break;
			}
		}
	}
	setIndexToNextEmptySlot();
}

UINT8* TermsAndConditions::getID()
{
	return tnc_id;
}

void TermsAndConditions::printall()
{
	for (UINT32 i = 0; i < translations; i++)
	{
		if(allTranslations[i] == NULL)
		{
			printTranslation(allTranslations[i]);
		}
	}
}

void TermsAndConditions::setIndexToNextEmptySlot()
{
	for (UINT32 i = 0; i < translations; i++)
	{
		if(allTranslations[i] == NULL)
		{
			currentTranslationIndex = i;
			return;
		}
	}
	/* Nothing is free */
	currentTranslationIndex = translations;
}
