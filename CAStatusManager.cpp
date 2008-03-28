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
 * 
 * This logic also describes the state machine of the mix
 * for networking, payment and system status
 */
CAStatusManager *CAStatusManager::ms_pStatusManager = NULL;

/** transitions for the networking states
    the networking events are:
		* ev_net_firstMixInited, 
		* ev_net_middleMixInited, 
		* ev_net_lastMixInited,
		* ev_net_prevConnected, 
		* ev_net_nextConnected, 
		* ev_net_prevConnectionClosed, 
		* ev_net_nextConnectionClosed,
**/

/* transitions for the networking entry state: */
#define TRANS_NET_ENTRY \
	(defineTransitions(stat_networking, 3, \
			ev_net_firstMixInited, st_net_firstMixInit, \
			ev_net_middleMixInited, st_net_middleMixInit, \
			ev_net_lastMixInited, st_net_lastMixInit))

/* transitions for st_net_firstMixInit: */
#define TRANS_NET_FIRST_MIX_INIT \
	(defineTransitions(stat_networking, 1, \
			ev_net_nextConnected, st_net_firstMixConnectedToNext))

/* transitions for st_net_firstMixConnectedToNext: */							
#define TRANS_NET_FIRST_MIX_CONNECTED_TO_NEXT \
	(defineTransitions(stat_networking, 2, \
			ev_net_nextConnected, st_net_firstMixOnline, \
			ev_net_nextConnectionClosed, st_net_firstMixInit))

/* transitions for st_net_firstMixOnline: */
#define TRANS_NET_FIRST_MIX_ONLINE \
	(defineTransitions(stat_networking, 1, \
			ev_net_nextConnectionClosed, st_net_firstMixInit))

/* transitions for st_net_middleMixInit: */
#define TRANS_NET_MIDDLE_MIX_INIT \
	(defineTransitions(stat_networking, 1, \
			ev_net_prevConnected, st_net_middleMixConnectedToPrev))

/* transitions for st_net_middleMixConnectedToPrev: */
#define TRANS_NET_MIDDLE_MIX_CONNECTED_TO_PREV \
	(defineTransitions(stat_networking, 1, \
			ev_net_nextConnected, st_net_middleMixOnline))
			
/* transitions for st_net_middleMixOnline: */
#define TRANS_NET_MIDDLE_MIX_ONLINE \
	(defineTransitions(stat_networking, 2, \
			ev_net_prevConnectionClosed, st_net_middleMixInit, \
			ev_net_nextConnectionClosed, st_net_middleMixInit))
			
/* transitions for st_net_lastMixInit: */
#define TRANS_NET_LAST_MIX_INIT \
	(defineTransitions(stat_networking, 1, \
			ev_net_prevConnected, st_net_lastMixOnline))

/* transitions for st_net_lastMixOnline: */
#define TRANS_NET_LAST_MIX_ONLINE \
	(defineTransitions(stat_networking, 1, \
			ev_net_prevConnectionClosed, st_net_lastMixInit))

static state_t networking_states[NR_NETWORKING_STATES] = 
{
		{st_net_entry, stat_networking, 
		 "networking entry state", 
		  NULL, NULL, TRANS_NET_ENTRY},
		
		{st_net_firstMixInit, stat_networking, 
		 "first mix initialized", 
		 NULL, NULL, TRANS_NET_FIRST_MIX_INIT},
		
		 {st_net_firstMixConnectedToNext, stat_networking, 
		  "first mix connected to next mix", 
		  NULL, NULL, TRANS_NET_FIRST_MIX_CONNECTED_TO_NEXT},
		 
		{st_net_firstMixOnline, stat_networking, 
		 "first mix online", 
		 NULL, NULL, TRANS_NET_FIRST_MIX_ONLINE},
		
		{st_net_middleMixInit, stat_networking,
		 "middle mix initialized", 
		 NULL, NULL, TRANS_NET_MIDDLE_MIX_INIT},
		
		{st_net_middleMixConnectedToPrev, stat_networking,
		 "middle mix connected to previous mix", 
		 NULL, NULL, TRANS_NET_MIDDLE_MIX_CONNECTED_TO_PREV},
		
		{st_net_middleMixOnline, stat_networking,
		 "middle mix online", 
		 NULL, NULL, TRANS_NET_MIDDLE_MIX_ONLINE},
		
		{st_net_lastMixInit, stat_networking,
		 "last mix initialized", 
		 NULL, NULL, TRANS_NET_LAST_MIX_INIT}, 
		
		{st_net_lastMixOnline, stat_networking, 
		 "last mix online", 
		 NULL, NULL, TRANS_NET_LAST_MIX_ONLINE}
};

static state_t payment_states[NR_PAYMENT_STATES] =
{
		{st_pay_entry, stat_payment, "payment entry state", NULL, NULL}
};

static state_t system_states[NR_SYSTEM_STATES] =
{
		{st_sys_entry, stat_system, "system entry state", NULL, NULL}
};

static state_t *all_states[NR_STATUS_TYPES] =
{
		networking_states, payment_states, system_states
};

static event_t networking_events[NR_NETWORKING_EVENTS] =
{
	{ev_net_firstMixInited, stat_networking,
	 "first mix initialization finished"},
		
	{ev_net_middleMixInited, stat_networking,
	 "middle mix initialization finished"},
		 
	{ev_net_lastMixInited, stat_networking,
	 "last mix initialization finished"},
		
	{ev_net_prevConnected, stat_networking,
	 "connection to previous mix established"},
		 
	{ev_net_nextConnected, stat_networking,
	 "connection to next mix established"},
		
	{ev_net_prevConnectionClosed, stat_networking,
	 "connection to previous mix closed"},
		
	{ev_net_nextConnectionClosed, stat_networking,
	 "connection to next mix closed"}
};

static event_t payment_events[NR_PAYMENT_EVENTS] =
{
		{ev_pay_dummy, stat_payment,
		 ""}
};

static event_t system_events[NR_SYSTEM_EVENTS] =
{
		{ev_sys_dummy, stat_system,
		 ""}		
};

static event_t *all_events[NR_STATUS_TYPES] =
{
		networking_events, payment_events, system_events
};

void CAStatusManager::init()
{
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
	m_pCurrentStates = new state_t*[NR_STATUS_TYPES];
	
	m_pCurrentStates[stat_networking] = 
		&(all_states[stat_networking][st_net_entry]);
	m_pCurrentStates[stat_payment] = 
		&(all_states[stat_payment][st_pay_entry]);
	m_pCurrentStates[stat_system] = 
		&(all_states[stat_system][st_sys_entry]);
	CAMsg::printMsg(LOG_DEBUG, "Init states: %s - %s, %s - %s, %s - %s\n",
			STATUS_NAMES[stat_networking], m_pCurrentStates[stat_networking]->st_description,
			STATUS_NAMES[stat_payment], m_pCurrentStates[stat_payment]->st_description,
			STATUS_NAMES[stat_system], m_pCurrentStates[stat_system]->st_description);
	
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
}

CAStatusManager::~CAStatusManager()
{
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
		m_pCurrentStates[s_type] = &(all_states[s_type][transitionToNextState]);
		m_pCurrentStates[s_type]->st_prev = prev;
		m_pCurrentStates[s_type]->st_cause = &(all_events[s_type][e_type]);
		
		/* setting the xml elements of the info message won't be too expensive */ 
		setDOMElementValue(
				(m_pCurrentStatesInfo[s_type]).dsi_stateType,
				(UINT32)(m_pCurrentStates[s_type])->st_type);
		setDOMElementValue(
				(m_pCurrentStatesInfo[s_type]).dsi_stateDesc,
				(UINT8*)(m_pCurrentStates[s_type])->st_description);
		
		CAMsg::printMsg(LOG_INFO, 
				"StatusManager: status %s: "
				"transition from state %d (%s) "
				"to state %d (%s) caused by event %d (%s)\n",
				STATUS_NAMES[s_type],
				prev->st_type, prev->st_description,
				m_pCurrentStates[s_type]->st_type, m_pCurrentStates[s_type]->st_description,
				e_type, (all_events[s_type][e_type]).ev_description);
	}
	m_pStatusLock->unlock();

	
	return E_SUCCESS;
}

/* prepares (once) a DOM template for all status messages */
SINT32 CAStatusManager::initStatusMessage()
{
	int i = 0;
	
	m_pPreparedStatusMessage = createDOMDocument();
	m_pCurrentStatesInfo = new dom_state_info[NR_STATUS_TYPES];
	DOMElement *status_dom_elements[NR_STATUS_TYPES];
	DOMElement *elemRoot = createDOMElement(m_pPreparedStatusMessage, "StatusMessage");
		
	status_dom_elements[stat_networking] = createDOMElement(m_pPreparedStatusMessage, "NetworkingStatus");
	status_dom_elements[stat_payment] = createDOMElement(m_pPreparedStatusMessage, "PaymentStatus");
	status_dom_elements[stat_system] = createDOMElement(m_pPreparedStatusMessage, "SystemStatus");
	
	for(; i < NR_STATUS_TYPES; i++)
	{
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
		status_dom_elements[i]->appendChild((m_pCurrentStatesInfo[i]).dsi_stateType);
		status_dom_elements[i]->appendChild((m_pCurrentStatesInfo[i]).dsi_stateLevel);
		status_dom_elements[i]->appendChild((m_pCurrentStatesInfo[i]).dsi_stateDesc);
		
		elemRoot->appendChild(status_dom_elements[i]);
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
				
			monitoringRequestSocket.send(statusMessageOutBuf, stausMessageOutBufLen);
			monitoringRequestSocket.close();
		}
		else
		{
			CAMsg::printMsg(LOG_ERR, 
					"StatusManager: error could not process monitoring request.\n");
		}
	}
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
