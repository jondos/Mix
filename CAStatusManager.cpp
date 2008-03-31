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
#include "CAStatusManager.hpp"
#include "CAMsg.hpp"
#include "CAMutex.hpp"
#include "CAUtil.hpp"
#include "xml/DOM_Output.hpp"

/**
 * @author Simon Pecher, JonDos GmbH
 * 
 * Here we keep track of the Mix states to be able to answer
 * Server monitoring requests.
 */
CAStatusManager *CAStatusManager::ms_pStatusManager = NULL;

state_t ***CAStatusManager::ms_pAllStates = NULL;
event_t ***CAStatusManager::ms_pAllEvents = NULL;

void CAStatusManager::init()
{
	if(ms_pAllEvents == NULL)
	{
		initEvents();
	}
	if(ms_pAllStates == NULL)
	{
		initStates();
	}
	if(ms_pStatusManager == NULL)
	{
		ms_pStatusManager = new CAStatusManager();
	}
}
	
void CAStatusManager::cleanup()
{
	CAMsg::printMsg(LOG_INFO, 
				"CAStatusManager: doing cleanup\n");
	if(ms_pStatusManager->m_pStatusSocket != NULL)
	{
		/*if(!(ms_pStatusManager->m_pStatusSocket->isClosed()))
		{
			ms_pStatusManager->m_pStatusSocket->close();
		}*/
	}
	
	if(ms_pStatusManager != NULL)
	{
		delete ms_pStatusManager;
		ms_pStatusManager = NULL;
	}
	if(ms_pAllStates != NULL)
	{
		deleteStates();
		ms_pAllStates = NULL;
	}
	if(ms_pAllEvents != NULL)
	{
		deleteEvents();
		ms_pAllEvents = NULL;
	}
}

SINT32 CAStatusManager::fireEvent(event_type_t e_type, enum status_type s_type)
{
	if(ms_pStatusManager != NULL)
	{
		return ms_pStatusManager->transition(e_type, s_type);
	}
	else
	{
		CAMsg::printMsg(LOG_CRIT, 
				"StatusManager: cannot handle event %d/%d because there is no StatusManager deployed\n", 
				s_type, e_type);
		return E_UNKNOWN;
	}
}

CAStatusManager::CAStatusManager()
{
	int i;
	m_pCurrentStates = new state_t*[NR_STATUS_TYPES];
	
	for(i = 0; i < NR_STATUS_TYPES; i++)
	{
		m_pCurrentStates[i] = 
				ms_pAllStates[i][ENTRY_STATE];
//#ifdef DEBUG
		CAMsg::printMsg(LOG_DEBUG, "Init state: %s - %s\n", STATUS_NAMES[i], 
						m_pCurrentStates[i]->st_description);
//#endif
	}
	
	m_pStatusLock = new CAMutex();
	m_pStatusSocket = new CASocket();

	if(initSocket() == E_SUCCESS)
	{
		
		m_pMonitoringThread = new CAThread((UINT8*)"Monitoring Thread");
		m_pMonitoringThread->setMainLoop(serveMonitoringRequests);
		m_pMonitoringThread->start(this);
		
	}
	else
	{
		CAMsg::printMsg(LOG_ERR, 
					"StatusManager: an error occured while initializing the"
					" server monitoring socket\n");
	}
	initStatusMessage();
	for(i = 0; i < NR_STATUS_TYPES; i++)
	{
		setDOMElementValue(
				(m_pCurrentStatesInfo[i]).dsi_stateType,
				(UINT32)(m_pCurrentStates[i])->st_type);
		setDOMElementValue(
				(m_pCurrentStatesInfo[i]).dsi_stateDesc,
				(UINT8*)(m_pCurrentStates[i])->st_description);
		setDOMElementValue(
				(m_pCurrentStatesInfo[i]).dsi_stateLevel,
				(UINT8*)(STATUS_LEVEL_NAMES[(m_pCurrentStates[i])->st_stateLevel]));

	}
}

CAStatusManager::~CAStatusManager()
{
	int i;
	if(m_pMonitoringThread != NULL)
	{
		if(m_pStatusSocket != NULL)
		{
			if( !(m_pStatusSocket->isClosed()) )
			{
				m_pStatusSocket->close();
			}
		}
		/*CAMsg::printMsg(LOG_INFO, 
					"CAStatusManager: wait for monitoring thread\n");
		m_pMonitoringThread->join();
		CAMsg::printMsg(LOG_INFO, 
				"CAStatusManager: monitoring thread joined\n");*/
		delete m_pMonitoringThread;
		CAMsg::printMsg(LOG_INFO, 
				"CAStatusManager: The monitoring thread is no more.\n");
		m_pMonitoringThread = NULL;
	}
	if(m_pStatusLock != NULL)
	{
		delete m_pStatusLock;
		m_pStatusLock = NULL;
	}
	if(m_pCurrentStates != NULL)
	{
		for(i = 0; i < NR_STATUS_TYPES; i++)
		{
			m_pCurrentStates[i] = NULL;
		}
		delete[] m_pCurrentStates;
		m_pCurrentStates = NULL;
	}
	if(m_pStatusSocket != NULL)
	{
		delete m_pStatusSocket;
		m_pStatusSocket = NULL;
	}
	/*if(m_pPreparedStatusMessage != NULL)
	{
		CAMsg::printMsg(LOG_INFO, 
						"CAStatusManager: before XML release.\n");
		m_pPreparedStatusMessage->release();
		m_pPreparedStatusMessage = NULL;
		CAMsg::printMsg(LOG_INFO, 
						"CAStatusManager: after XML release.\n");
	}*/
}
SINT32 CAStatusManager::initSocket()
{
	SINT32 ret;
	if(m_pStatusSocket == NULL)
	{
		m_pStatusSocket = new CASocket();
	}
	
	if( !(m_pStatusSocket->isClosed()) )
	{
		CAMsg::printMsg(LOG_ERR, 
				"StatusManager: server monitoring socket already connected.\n");
		return E_UNKNOWN;
	}
	
	ret = m_pStatusSocket->create();
	if(ret != E_SUCCESS)
	{
		CAMsg::printMsg(LOG_ERR, 
				"StatusManager: could not create server monitoring socket.\n");
		return ret;
	}
	
	ret = m_pStatusSocket->listen(MONITORING_SERVER_PORT);
	if(ret != E_SUCCESS)
	{
		CAMsg::printMsg(LOG_ERR, 
				"StatusManager: not able to init server socket for monitoring.\n");
		return ret;
	}
	return E_SUCCESS;
}

SINT32 CAStatusManager::transition(event_type_t e_type, status_type_t s_type)
{
	transition_t transitionToNextState;
	state_t *prev = NULL;
	
	if( (m_pStatusLock == NULL) ||
		(m_pCurrentStates == NULL) )
	{
		CAMsg::printMsg(LOG_CRIT, 
				"StatusManager: fatal error\n");
		return E_UNKNOWN;
	}
	if((s_type >= NR_STATUS_TYPES) || (s_type < 0))
	{
		CAMsg::printMsg(LOG_ERR, 
				"StatusManager: received event for an invalid status type: %d\n", s_type);
		return E_INVALID;
	}
	if((e_type >= EVENT_COUNT[s_type]) || (e_type < 0))
	{
		CAMsg::printMsg(LOG_ERR, 
				"StatusManager: received an invalid event: %d\n", e_type);
		return E_INVALID;
	}
	
	/* We process incoming events synchronously, so the calling thread
	 * should perform state transition very quickly to avoid long blocking 
	 * of other calling threads
	 */
	m_pStatusLock->lock();
	if(m_pCurrentStates[s_type]->st_transitions == NULL)
	{
		m_pStatusLock->unlock();
		CAMsg::printMsg(LOG_CRIT, 
						"StatusManager: current state is corrupt\n");
		return E_UNKNOWN; 
	}
	transitionToNextState = m_pCurrentStates[s_type]->st_transitions[e_type];
	if(transitionToNextState >= STATE_COUNT[s_type])
	{
		m_pStatusLock->unlock();
		CAMsg::printMsg(LOG_ERR, 
						"StatusManager: transition to invalid state %d\n", transitionToNextState);
		return E_INVALID;
	}
	if(transitionToNextState != st_ignore)
	{
		prev = m_pCurrentStates[s_type];
		m_pCurrentStates[s_type] = ms_pAllStates[s_type][transitionToNextState];
		m_pCurrentStates[s_type]->st_prev = prev;
		m_pCurrentStates[s_type]->st_cause = ms_pAllEvents[s_type][e_type];
		
		/* setting the xml elements of the info message won't be too expensive */ 
		setDOMElementValue(
				(m_pCurrentStatesInfo[s_type]).dsi_stateType,
				(UINT32)(m_pCurrentStates[s_type])->st_type);
		setDOMElementValue(
				(m_pCurrentStatesInfo[s_type]).dsi_stateDesc,
				(UINT8*)(m_pCurrentStates[s_type])->st_description);
		setDOMElementValue(
				(m_pCurrentStatesInfo[s_type]).dsi_stateLevel,
				(UINT8*)(STATUS_LEVEL_NAMES[(m_pCurrentStates[s_type])->st_stateLevel]));
//#ifdef DEBUG
		CAMsg::printMsg(LOG_INFO, 
				"StatusManager: status %s: "
				"transition from state %d (%s) "
				"to state %d (%s) caused by event %d (%s)\n",
				STATUS_NAMES[s_type],
				prev->st_type, prev->st_description,
				m_pCurrentStates[s_type]->st_type, m_pCurrentStates[s_type]->st_description,
				e_type, (ms_pAllEvents[s_type][e_type])->ev_description);
//#endif
	}
	m_pStatusLock->unlock();
	return E_SUCCESS;
}

/* prepares (once) a DOM template for all status messages */
SINT32 CAStatusManager::initStatusMessage()
{
	int i;
	
	m_pPreparedStatusMessage = createDOMDocument();
	m_pCurrentStatesInfo = new dom_state_info[NR_STATUS_TYPES];
	DOMElement *elemRoot = createDOMElement(m_pPreparedStatusMessage, "StatusMessage");
	DOMElement *status_dom_element = NULL;
	
	for(i = 0; i < NR_STATUS_TYPES; i++)
	{
		status_dom_element = 
			createDOMElement(m_pPreparedStatusMessage, STATUS_NAMES[i]); 
		(m_pCurrentStatesInfo[i]).dsi_stateType = 
			createDOMElement(m_pPreparedStatusMessage, "State");
#ifdef DEBUG		
		setDOMElementValue((m_pCurrentStatesInfo[i]).dsi_stateType, (UINT8*)"Statenumber");
#endif		
		(m_pCurrentStatesInfo[i]).dsi_stateLevel =
			createDOMElement(m_pPreparedStatusMessage, "StateLevel");
#ifdef DEBUG
		setDOMElementValue((m_pCurrentStatesInfo[i]).dsi_stateLevel, (UINT8*)"OK or Critcal or something");
#endif
		(m_pCurrentStatesInfo[i]).dsi_stateDesc =
			createDOMElement(m_pPreparedStatusMessage, "StateDescription");
#ifdef DEBUG
		setDOMElementValue((m_pCurrentStatesInfo[i]).dsi_stateDesc, (UINT8*)"Description of the state");
#endif
		status_dom_element->appendChild((m_pCurrentStatesInfo[i]).dsi_stateType);
		status_dom_element->appendChild((m_pCurrentStatesInfo[i]).dsi_stateLevel);
		status_dom_element->appendChild((m_pCurrentStatesInfo[i]).dsi_stateDesc);
		
		elemRoot->appendChild(status_dom_element);
	}
	m_pPreparedStatusMessage->appendChild(elemRoot);

#ifdef DEBUG
	UINT32 debuglen = 3000;
	UINT8 debugout[3000];
	DOM_Output::dumpToMem(m_pPreparedStatusMessage,debugout,&debuglen);
	debugout[debuglen] = 0;			
	CAMsg::printMsg(LOG_DEBUG, "the status message template looks like this: %s \n",debugout);
#endif
	
	return E_SUCCESS;
}

THREAD_RETURN serveMonitoringRequests(void* param)
{
	CASocket monitoringRequestSocket;
	CAStatusManager *statusManager = (CAStatusManager*) param;
	for(;EVER;)
	{
		if(statusManager->m_pStatusSocket != NULL)
		{
			if(statusManager->m_pStatusSocket->isClosed())
			{
				CAMsg::printMsg(LOG_INFO, 
							"Monitoring Thread: server socket closed, leaving loop.\n");
				THREAD_RETURN_SUCCESS;
			}
		}
		else
		{
			CAMsg::printMsg(LOG_ERR, 
						"Monitoring Thread: server socket disposed, leaving loop.\n");
			THREAD_RETURN_ERROR;
		}
		
		if(statusManager->m_pStatusSocket->accept(monitoringRequestSocket) == E_SUCCESS)
		{
			UINT32 stausMessageOutBufLen = 3000;
			UINT8 statusMessageOutBuf[3000];
			
			statusManager->m_pStatusLock->lock();
			DOM_Output::dumpToMem(
					statusManager->m_pPreparedStatusMessage,
					statusMessageOutBuf,
					&stausMessageOutBufLen);
			statusManager->m_pStatusLock->unlock();
			
			statusMessageOutBuf[stausMessageOutBufLen] = 0;			
			//CAMsg::printMsg(LOG_DEBUG, "the status message looks like this: %s \n",debugout);
				
			if(monitoringRequestSocket.send(statusMessageOutBuf, stausMessageOutBufLen) < 0)
			{
				CAMsg::printMsg(LOG_ERR, 
						"StatusManager: error: could not send status message.\n");
			}
			monitoringRequestSocket.close();
		}
		else
		{
			CAMsg::printMsg(LOG_ERR, 
					"StatusManager: error: could not process monitoring request.\n");
		}
	}
}

void CAStatusManager::initStates()
{
	int i, j; 
	
	ms_pAllStates = new state_t**[NR_STATUS_TYPES];
	
	for(i = 0; i < NR_STATUS_TYPES; i++)
	{
		ms_pAllStates[i] = 
			new state_t*[STATE_COUNT[i]];
		
		for(j=0; j < STATE_COUNT[i]; j++)
		{
			ms_pAllStates[i][j] = new state_t; 
			/* only state identifier are set, transitions and state description
			 * must be set via macro
			 **/
			ms_pAllStates[i][j]->st_type = (state_type_t) j;
			ms_pAllStates[i][j]->st_statusType = (status_type_t) i;
		}
	}
	FINISH_STATE_DEFINITIONS(ms_pAllStates);
	
}

void CAStatusManager::deleteStates()
{
	int i, j;
	
	for(i = 0; i < NR_STATUS_TYPES; i++)
	{
		//m_pCurrentStates[i] = NULL;
		for(j=0; j < STATE_COUNT[i]; j++)
		{
			if(ms_pAllStates[i][j] != NULL)
			{
				if(ms_pAllStates[i][j]->st_transitions != NULL)
				{
					delete[] ms_pAllStates[i][j]->st_transitions;
					ms_pAllStates[i][j]->st_transitions = NULL;
				}
				//todo: delete state descriptions ?
				delete ms_pAllStates[i][j];
				ms_pAllStates[i][j] = NULL;
			}
		}
		delete[] ms_pAllStates[i];
	}
	delete[] ms_pAllStates;
}

void CAStatusManager::initEvents()
{
	int i , j;
	
	ms_pAllEvents = new event_t**[NR_STATUS_TYPES];
	for(i = 0; i < NR_STATUS_TYPES; i++)
	{
		ms_pAllEvents[i] = new event_t*[EVENT_COUNT[i]];
		for(j = 0; j < EVENT_COUNT[i]; j++)
		{
			ms_pAllEvents[i][j] = new event_t;
			ms_pAllEvents[i][j]->ev_type = (event_type_t) j;
			ms_pAllEvents[i][j]->ev_statusType = (status_type_t) i;
		}
	}
	FINISH_EVENT_DEFINITIONS(ms_pAllEvents);
}

void CAStatusManager::deleteEvents()
{
	int i , j;
		for(i = 0; i < NR_STATUS_TYPES; i++)
		{
			for(j = 0; j < EVENT_COUNT[i]; j++)
			{
				//todo: delete event descriptions ?
				delete ms_pAllEvents[i][j];
				ms_pAllEvents[i][j] = NULL;
			}
			delete[] ms_pAllEvents[i];
		}
		delete[] ms_pAllEvents;
}

transition_t *defineTransitions(status_type_t s_type, SINT32 transitionCount, ...)
{
	va_list ap;
	transition_t *transitions = NULL;
	int i;
	event_type_t specifiedEventTypes[transitionCount];
	transition_t specifiedTransitions[transitionCount];
	
	/* read in the specified events with the correspondig transitions */
	va_start(ap, transitionCount);
	for(i = 0; i < transitionCount; i++)
	{
		specifiedEventTypes[i] = (event_type_t) va_arg(ap, int);
		specifiedTransitions[i] = (transition_t) va_arg(ap, int);
	}
	va_end(ap);
	
	if((s_type >= NR_STATUS_TYPES) || (s_type < 0))
	{
		/* invalid status type specified */
		return NULL;
	}
	if(transitionCount > (EVENT_COUNT[s_type]))
	{
		/* more transitions specified than events defined*/ 
		return NULL;
	}
	
	transitions = new transition_t[(EVENT_COUNT[s_type])];
	memset(transitions, st_ignore, (sizeof(transition_t)*(EVENT_COUNT[s_type])));
	for(i = 0; i < transitionCount; i++)
	{
		if((specifiedEventTypes[i] >= EVENT_COUNT[s_type]) || 
			(specifiedEventTypes[i] < 0)) 
		{
			/* specified event is invalid */
			CAMsg::printMsg(LOG_WARNING, 
						"StatusManager: definiton of an invalid state transition (invalid event %d).\n",
						specifiedEventTypes[i]);
			continue;
		}
		if((specifiedTransitions[i] >= STATE_COUNT[s_type]) ||
			(specifiedTransitions[i] < st_ignore))
		{
			/* corresponding transition to event is not valid */
			CAMsg::printMsg(LOG_WARNING, 
						"StatusManager: definiton of an invalid state transition (invalid state %d).\n",
						specifiedTransitions[i]);
			continue;
		}
		transitions[specifiedEventTypes[i]] = specifiedTransitions[i];
	}
	/*for(i=0; i < (EVENT_COUNT[s_type]); i++)
	{
		CAMsg::printMsg(LOG_DEBUG, "transitions %d: %d\n", i, transitions[i]);
	}*/
	return transitions;
	
}
