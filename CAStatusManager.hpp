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
#ifndef CASTATUSMANAGER_HPP_
#define CASTATUSMANAGER_HPP_

/**
 * @author Simon Pecher, JonDos GmbH
 */

#include "StdAfx.h"
#include "CASocket.hpp"
#include "CAThread.hpp"

/* How many different status types do exist*/
#define NR_STATUS_TYPES 3

#define NETWORKING_STATUS_NAME "Networking"
#define PAYMENT_STATUS_NAME "Payment"
#define SYSTEM_STATUS_NAME "System"

/* How many different events do exist for each status type*/
#define NR_NETWORKING_EVENTS 7
#define NR_PAYMENT_EVENTS 1
#define NR_SYSTEM_EVENTS 1

#define NR_NETWORKING_STATES 9
#define NR_PAYMENT_STATES 1
#define NR_SYSTEM_STATES 1

#define MAX_DESCRIPTION_LENGTH 50

#define MONITORING_SERVER_PORT 8080

#define EVER 1

static const SINT32 EVENT_COUNT[NR_STATUS_TYPES] =
{
		NR_NETWORKING_EVENTS, NR_PAYMENT_EVENTS, NR_SYSTEM_EVENTS
};

static const SINT32 STATE_COUNT[NR_STATUS_TYPES] =
{
		NR_NETWORKING_STATES, NR_PAYMENT_STATES, NR_SYSTEM_STATES
};

static const char *STATUS_NAMES[NR_STATUS_TYPES] =
{
		NETWORKING_STATUS_NAME, PAYMENT_STATUS_NAME, SYSTEM_STATUS_NAME
};

enum state_type
{
	st_ignore = -1,
	/* networking states */
	st_net_entry,
	st_net_firstMixInit, st_net_firstMixConnectedToNext, st_net_firstMixOnline , 
	st_net_middleMixInit, st_net_middleMixConnectedToPrev, st_net_middleMixOnline,
	st_net_lastMixInit, st_net_lastMixOnline,
	
	/* payment states */
	st_pay_entry = 0,
	
	/* system states */
	st_sys_entry = 0
};

enum status_type
{
	stat_networking = 0, stat_payment, stat_system 
};

typedef enum status_type status_type_t;

enum event_type
{
	/* networking events */
	ev_net_firstMixInited = 0, ev_net_middleMixInited , ev_net_lastMixInited,
	ev_net_prevConnected, ev_net_nextConnected, 
	ev_net_prevConnectionClosed, ev_net_nextConnectionClosed,
	
	/* payment events */
	ev_pay_dummy = 0,
	
	/* system states */
	ev_sys_dummy = 0
};

typedef enum event_type event_type_t;

struct event
{
	event_type_t ev_type;
	status_type_t ev_statusType;
	char ev_description[MAX_DESCRIPTION_LENGTH];
};

typedef struct event event_t;
typedef enum state_type transition_t;

struct state
{
	enum state_type st_type;
	status_type_t st_statusType;
	char st_description[MAX_DESCRIPTION_LENGTH];
	struct event *st_cause;
	struct state *st_prev;
	transition_t *st_transitions;
};

typedef struct state state_t;

struct dom_state_info
{
	DOMElement *dsi_stateType;
	DOMElement *dsi_stateLevel;
	DOMElement *dsi_stateDesc;
};

typedef struct dom_state_info dom_state_info_t;

/** 
 * the server routine which: 
 *  * accepts socket connections,
 *  * outputs a status message
 *  * and closes the socket immediatley after that
 */
THREAD_RETURN serveMonitoringRequests(void* param);

/**
 * a conveinience function for easily defining state transitions
 * @param transitions the array where to store the transitions
 * @param s_type the status type of the state for which the transitions 
 * 		  are to be defined
 * @param transitionCount the number of transitions to define
 * @param ... an event_type (of type event_type_t) followed by a 
 * 		  state transition (of type transition_t) 
 * 		  IMPORTANT an event type MUST be followed by a state transition!
 */
transition_t *defineTransitions(status_type_t s_type, SINT32 transitionCount, ...);

class CAStatusManager
{

public:
	
	static void init();
	static void cleanup();

	static SINT32 fireEvent(event_type_t e_type, status_type_t s_type);

private:
	
	state_t **m_pCurrentStates;
	dom_state_info_t *m_pCurrentStatesInfo;
	CAMutex *m_pStatusLock;
	CASocket *m_pStatusSocket;
	CAThread *m_pMonitoringThread;
	
	XERCES_CPP_NAMESPACE::DOMDocument* m_pPreparedStatusMessage;
	
	static CAStatusManager *ms_pStatusManager;
	
	CAStatusManager();
	virtual ~CAStatusManager();
	
	SINT32 initSocket();
	SINT32 transition(event_type_t e_type, status_type_t s_type);
	SINT32 initStatusMessage();
	friend THREAD_RETURN serveMonitoringRequests(void* param);
};

#endif /*CASTATUSMANAGER_HPP_*/
