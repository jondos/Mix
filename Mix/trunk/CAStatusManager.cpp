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

/** transitions for the defined states **/

/* state transition when the corresponing events from enumeration occurs, the order is the same:
 * ev_net_firstMixInited, ev_net_middleMixInited , ev_net_lastMixInited, 
 * ev_net_prevConnected, ev_net_nextConnected, 
 * ev_net_prevConnectionClosed, ev_net_nextConnectionClosed 
 */

/* transitions for the networking entry state: */
enum state_type trans_net_entry[NR_NETWORKING_EVENTS] =
{
	st_net_firstMixInit, st_net_middleMixInit, st_net_lastMixInit, 
	st_ignore, st_ignore, 
	st_ignore, st_ignore
};

/* transitions for st_net_firstMixInit: */
enum state_type trans_net_firstMixInit[NR_NETWORKING_EVENTS] =
{
	st_ignore, st_ignore, st_ignore, 
	st_ignore, st_net_firstMixConnectedToNext, 
	st_ignore, st_ignore
};

/* transitions for st_net_firstMixConnectedToNext: */
enum state_type trans_net_firstMixConnectedToNext[NR_NETWORKING_EVENTS] =
{
	st_ignore, st_ignore, st_ignore, 
	st_ignore, st_net_firstMixOnline, 
	st_ignore, st_net_firstMixInit
};

/* transitions for st_net_firstMixOnline: */
enum state_type trans_net_firstMixOnline[NR_NETWORKING_EVENTS] =
{
	st_ignore, st_ignore, st_ignore, 
	st_ignore, st_ignore, 
	st_ignore, st_net_firstMixInit
};

/* transitions for st_net_middleMixInit: */
enum state_type trans_net_middleMixInit[NR_NETWORKING_EVENTS] =
{
	st_ignore, st_ignore, st_ignore, 
	st_net_middleMixConnectedToPrev, st_ignore, 
	st_ignore, st_ignore
};

/* transitions for st_net_middleMixConnectedToPrev: */
enum state_type trans_net_middleMixConnectedToPrev[NR_NETWORKING_EVENTS] =
{
	st_ignore, st_ignore, st_ignore, 
	st_ignore, st_net_middleMixOnline, 
	st_net_middleMixInit, st_ignore
};

/* transitions for st_net_middleMixOnline: */
enum state_type trans_net_middleMixOnline[NR_NETWORKING_EVENTS] =
{
	st_ignore, st_ignore, st_ignore, 
	st_ignore, st_ignore, 
	st_net_middleMixInit, st_net_middleMixInit
};

/* transitions for st_net_lastMixInit: */
enum state_type trans_net_lastMixInit[NR_NETWORKING_EVENTS] =
{
	st_ignore, st_ignore, st_ignore, 
	st_net_lastMixOnline, st_ignore, 
	st_ignore, st_ignore
};

/* transitions for st_net_lastMixOnline: */
enum state_type trans_net_lastMixOnline[NR_NETWORKING_EVENTS] =
{
	st_ignore, st_ignore, st_ignore, 
	st_ignore, st_ignore, 
	st_net_lastMixInit, st_ignore
};

static state_t networking_states[NR_NETWORKING_STATES] = 
{
		{st_net_entry, stat_networking, 
		 "networking entry state", 
		  NULL, NULL, trans_net_entry},
		
		{st_net_firstMixInit, stat_networking, 
		 "first mix initialized", 
		 NULL, NULL, trans_net_firstMixInit},
		
		 {st_net_firstMixConnectedToNext, stat_networking, 
		  "first mix connected to next mix", 
		  NULL, NULL, trans_net_firstMixConnectedToNext},
		 
		{st_net_firstMixOnline, stat_networking, 
		 "first mix online", 
		 NULL, NULL, trans_net_firstMixOnline},
		
		{st_net_middleMixInit, stat_networking,
		 "middle mix initialized", 
		 NULL, NULL, trans_net_middleMixInit},
		
		{st_net_middleMixConnectedToPrev, stat_networking,
		 "middle mix connected to previous mix", 
		 NULL, NULL, trans_net_middleMixConnectedToPrev},
		
		{st_net_middleMixOnline, stat_networking,
		 "middle mix online", 
		 NULL, NULL, trans_net_middleMixOnline},
		
		{st_net_lastMixInit, stat_networking,
		 "last mix initialized", 
		 NULL, NULL, trans_net_lastMixInit}, 
		
		{st_net_lastMixOnline, stat_networking, 
		 "last mix online", 
		 NULL, NULL, trans_net_lastMixOnline}
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

SINT32 CAStatusManager::fireEvent(enum event_type e_type, enum status_type s_type)
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
}
SINT32 CAStatusManager::initSocket()
{
	SINT32 ret;
	if(m_pStatusSocket == NULL)
	{
		CAMsg::printMsg(LOG_ERR, "0.1\n");
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

SINT32 CAStatusManager::transition(enum event_type e_type, enum status_type s_type)
{
	enum state_type next_state_type;
	state_t *prev = NULL;
	
	if( (m_pStatusLock == NULL) ||
		(m_pCurrentStates == NULL) )
	{
		CAMsg::printMsg(LOG_CRIT, 
				"StatusManager: fatal error\n");
		return E_UNKNOWN;
	}
	if(s_type >= NR_STATUS_TYPES)
	{
		CAMsg::printMsg(LOG_ERR, 
				"StatusManager: received event for an invalid status type: %d\n", s_type);
		return E_INVALID;
	}
	if(e_type >= EVENT_COUNT[s_type])
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
	next_state_type = m_pCurrentStates[s_type]->st_transitions[e_type];
	if(next_state_type >= STATE_COUNT[s_type])
	{
		m_pStatusLock->unlock();
		CAMsg::printMsg(LOG_ERR, 
						"StatusManager: transition to invalid state %d\n", next_state_type);
		return E_INVALID;
	}
	if(next_state_type != st_ignore)
	{
		prev = m_pCurrentStates[s_type];
		m_pCurrentStates[s_type] = &(all_states[s_type][next_state_type]);
		m_pCurrentStates[s_type]->st_prev = prev;
		m_pCurrentStates[s_type]->st_cause = &(all_events[s_type][e_type]);
		CAMsg::printMsg(LOG_INFO, 
						"StatusManager: status %s: "
						"transition from state %d (%s) "
						"to state %d (%s) caused by event %d (%s)\n",
						STATUS_NAMES[s_type],
						prev->st_type, prev->st_description,
						next_state_type, m_pCurrentStates[s_type]->st_description,
						e_type, (all_events[s_type][e_type]).ev_description);
	}
	m_pStatusLock->unlock();

	
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
			statusManager->m_pStatusLock->lock();
			monitoringRequestSocket.send(
					(UINT8*)(statusManager->m_pCurrentStates[stat_networking]), 
					MAX_DESCRIPTION_LENGTH);
			statusManager->m_pStatusLock->unlock();
			monitoringRequestSocket.close();
		}
		else
		{
			CAMsg::printMsg(LOG_ERR, 
					"StatusManager: error could not process monitoring request.\n");
		}
	}
}