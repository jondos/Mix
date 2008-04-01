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
 * 
 * Logic to describe and process the state machine of a mix
 * for reporting networking, payment and system status to server
 * monitoring systems
 */

#include "StdAfx.h"

#ifdef SERVER_MONITORING

#include "CASocket.hpp"
#include "CAThread.hpp"

/* How many different status types do exist*/
#ifdef PAYMENT
	#define NR_STATUS_TYPES 3

	#define PAYMENT_STATUS_NAME "PaymentStatus"
	#define NR_PAYMENT_STATES 1
	#define NR_PAYMENT_EVENTS 1
#else
	#define NR_STATUS_TYPES 2	
#endif
	
#define NETWORKING_STATUS_NAME "NetworkingStatus"
#define NR_NETWORKING_STATES 11
#define NR_NETWORKING_EVENTS 11

#define SYSTEM_STATUS_NAME "SystemStatus"
#define NR_SYSTEM_STATES 1
#define NR_SYSTEM_EVENTS 1

#define NR_STATE_LEVELS 2

#define ENTRY_STATE 0

#define MAX_DESCRIPTION_LENGTH 50

#define MONITORING_SERVER_PORT 8080

#define EVER 1

static const SINT32 EVENT_COUNT[NR_STATUS_TYPES] =
{
		NR_NETWORKING_EVENTS, 
#ifdef PAYMENT
		NR_PAYMENT_EVENTS, 
#endif
		NR_SYSTEM_EVENTS
};

static const SINT32 STATE_COUNT[NR_STATUS_TYPES] =
{
		NR_NETWORKING_STATES, 
#ifdef PAYMENT
		NR_PAYMENT_STATES, 
#endif
		NR_SYSTEM_STATES
};

static const char *STATUS_NAMES[NR_STATUS_TYPES] =
{
		NETWORKING_STATUS_NAME, 
#ifdef PAYMENT
		PAYMENT_STATUS_NAME, 
#endif		
		SYSTEM_STATUS_NAME
};

static const char *STATUS_LEVEL_NAMES[NR_STATE_LEVELS] =
{
		"OK", "Critical"
};

enum state_type
{
	st_ignore = -1,
	/* networking states */
	st_net_entry,
	st_net_firstMixInit, st_net_firstMixConnectedToNext, st_net_firstMixOnline , 
	st_net_middleMixInit, st_net_middleMixConnectedToPrev, 
	st_net_middleMixConnectedToNext, st_net_middleMixOnline,
	st_net_lastMixInit, st_net_lastMixConnectedToPrev, st_net_lastMixOnline,
#ifdef PAYMENT
	/* payment states */
	st_pay_entry = 0,
#endif
	/* system states */
	st_sys_entry = 0
};

enum status_type
{
	stat_networking = 0, 
#ifdef PAYMENT
	stat_payment, 
#endif
	stat_system 
};

enum event_type
{
	/* networking events */
	ev_net_firstMixInited = 0, ev_net_middleMixInited , ev_net_lastMixInited,
	ev_net_prevConnected, ev_net_nextConnected, 
	ev_net_prevConnectionClosed, ev_net_nextConnectionClosed,
	ev_net_keyExchangePrevSuccessful, ev_net_keyExchangeNextSuccessful,
	ev_net_keyExchangePrevFailed, ev_net_keyExchangeNextFailed,
#ifdef PAYMENT	
	/* payment events */
	ev_pay_dummy = 0,
#endif
	/* system states */
	ev_sys_dummy = 0
};

/* indexes must correspond to strings in STATUS_LEVEL_NAMES */
enum state_level
{
	stl_ok = 0, stl_critcial
};

typedef enum state_type state_type_t;
typedef enum status_type status_type_t;
typedef enum event_type event_type_t;
typedef enum state_level state_level_t;
typedef enum state_type transition_t;

struct event
{
	event_type_t ev_type;
	status_type_t ev_statusType;
	char *ev_description;
};

struct state
{
	state_type_t st_type;
	status_type_t st_statusType;
	state_level_t st_stateLevel;
	char *st_description;
	struct event *st_cause;
	struct state *st_prev;
	transition_t *st_transitions;
};

struct dom_state_info
{
	DOMElement *dsi_stateType;
	DOMElement *dsi_stateLevel;
	DOMElement *dsi_stateDesc;
};

typedef struct state state_t;
typedef struct event event_t;
typedef struct dom_state_info dom_state_info_t;

/** helper macros for defining states and events: **/
#define FINISH_STATE_DEFINITIONS(state_array) \
			FINISH_NETWORKING_STATE_DEFINITIONS(state_array) \
			FINISH_PAYMENT_STATE_DEFINITIONS(state_array) \
			FINISH_SYSTEM_STATE_DEFINITIONS(state_array)

#define FINISH_EVENT_DEFINITIONS(event_array) \
			FINISH_NETWORKING_EVENT_DEFINITIONS(event_array) \
			FINISH_PAYMENT_EVENT_DEFINITIONS(event_array) \
			FINISH_SYSTEM_EVENT_DEFINITIONS(event_array)

/* networking states description and transition assignment 
 * new networking state definitions can be appended here 
 * (after being declared as networking enum state_type)
 */
#define FINISH_NETWORKING_STATE_DEFINITIONS(state_array) \
			NET_STATE_DEF(state_array, st_net_entry, \
					"networking entry state", \
					TRANS_NET_ENTRY, stl_ok) \
			NET_STATE_DEF(state_array, st_net_firstMixInit,\
					"first mix initialized", \
					TRANS_NET_FIRST_MIX_INIT, stl_ok) \
			NET_STATE_DEF(state_array, st_net_firstMixConnectedToNext, \
					"first mix connected to next mix", \
					  TRANS_NET_FIRST_MIX_CONNECTED_TO_NEXT, stl_ok) \
			NET_STATE_DEF(state_array, st_net_firstMixOnline, \
					"first mix online", \
					 TRANS_NET_FIRST_MIX_ONLINE, stl_ok) \
			NET_STATE_DEF(state_array, st_net_middleMixInit, \
					"middle mix initialized", \
					TRANS_NET_MIDDLE_MIX_INIT, stl_ok) \
			NET_STATE_DEF(state_array, st_net_middleMixConnectedToPrev, \
					"middle mix connected to previous mix", \
					TRANS_NET_MIDDLE_MIX_CONNECTED_TO_PREV, stl_ok) \
			NET_STATE_DEF(state_array, st_net_middleMixConnectedToNext, \
					"middle mix connected to next mix", \
					TRANS_NET_MIDDLE_MIX_CONNECTED_TO_NEXT, stl_ok) \
			NET_STATE_DEF(state_array, st_net_middleMixOnline, \
					"middle mix online", \
					TRANS_NET_MIDDLE_MIX_ONLINE, stl_ok) \
			NET_STATE_DEF(state_array, st_net_lastMixInit, \
					"last mix initialized", \
					TRANS_NET_LAST_MIX_INIT, stl_ok) \
			NET_STATE_DEF(state_array, st_net_lastMixConnectedToPrev, \
					"last mix connected to previous mix", \
		  			TRANS_NET_LAST_MIX_CONNECTED_TO_PREV, stl_ok) \
		  	NET_STATE_DEF(state_array, st_net_lastMixOnline, \
		  			"last mix online", \
					TRANS_NET_LAST_MIX_ONLINE, stl_ok) 

/* networking events descriptions */
#define FINISH_NETWORKING_EVENT_DEFINITIONS(event_array) \
			NET_EVENT_DEF(event_array, ev_net_firstMixInited, \
					"first mix initialization finished") \
			NET_EVENT_DEF(event_array, ev_net_middleMixInited, \
					"middle mix initialization finished") \
			NET_EVENT_DEF(event_array, ev_net_lastMixInited, \
					"last mix initialization finished") \
			NET_EVENT_DEF(event_array, ev_net_prevConnected, \
					"connection to previous mix established") \
			NET_EVENT_DEF(event_array, ev_net_nextConnected, \
					"connection to next mix established") \
			NET_EVENT_DEF(event_array, ev_net_keyExchangePrevSuccessful, \
					"key exchange with previous mix successful") \
			NET_EVENT_DEF(event_array, ev_net_keyExchangeNextSuccessful, \
		  			"key exchange with next mix successful") \
  			NET_EVENT_DEF(event_array, ev_net_keyExchangePrevFailed, \
  					"key exchange with previous mix failed") \
			NET_EVENT_DEF(event_array, ev_net_keyExchangeNextFailed, \
		  			"key exchange with next mix failed") \
		  	NET_EVENT_DEF(event_array, ev_net_prevConnectionClosed, \
					"connection to previous mix closed") \
			NET_EVENT_DEF(event_array, ev_net_nextConnectionClosed, \
					"connection to next mix closed")
		
#ifdef PAYMENT
/* payment states description and transition assignment 
 * new payment state definitions can be appended here 
 * (after being declared as payment enum state_type)
 */
	#define FINISH_PAYMENT_STATE_DEFINITIONS(state_array) \
				PAY_STATE_DEF(state_array, st_pay_entry, \
						"payment entry state", \
						TRANS_PAY_DUMMY, stl_ok)
	
	/* payment events descriptions */
	#define FINISH_PAYMENT_EVENT_DEFINITIONS(event_array) \
				PAY_EVENT_DEF(event_array, ev_pay_dummy, \
						  "payment event")
#else
	#define FINISH_PAYMENT_STATE_DEFINITIONS(state_array) 
	#define FINISH_PAYMENT_EVENT_DEFINITIONS(event_array)
#endif

/* system states description and transition assignment 
 * new system state definitions can be appended here 
 * (after being declared as system enum state_type)
 */
#define FINISH_SYSTEM_STATE_DEFINITIONS(state_array) \
			SYS_STATE_DEF(state_array, st_sys_entry, \
					"system entry state", \
					TRANS_SYS_DUMMY, stl_ok)

/* payment events descriptions */
#define FINISH_SYSTEM_EVENT_DEFINITIONS(event_array) \
			SYS_EVENT_DEF(event_array, ev_sys_dummy, \
					  "system event")
					  
/* conveinience macros for special status state and event definitions */
#define NET_STATE_DEF(state_array, state_type, description, transitions, stateLevel) \
			STATE_DEF(state_array, stat_networking, state_type, description, transitions, stateLevel)

#define NET_EVENT_DEF(event_array, event_type, description) \
			EVENT_DEF(event_array, stat_networking, event_type, description)

#ifdef PAYMENT
	#define PAY_STATE_DEF(state_array, state_type, description, transitions, stateLevel) \
				STATE_DEF(state_array, stat_payment, state_type, description, transitions, stateLevel)
	
	#define PAY_EVENT_DEF(event_array, event_type, description) \
				EVENT_DEF(event_array, stat_payment, event_type, description)
#else
	#define PAY_STATE_DEF(state_array, state_type, description, transitions, stateLevel)
	#define PAY_EVENT_DEF(event_array, event_type, description)
#endif

#define SYS_STATE_DEF(state_array, state_type, description, transitions, stateLevel) \
			STATE_DEF(state_array, stat_system, state_type, description, transitions, stateLevel)

#define SYS_EVENT_DEF(event_array, event_type, description) \
			EVENT_DEF(event_array, stat_system, event_type, description)

/* This macro is used for assigning state description and state transitions 
 * to the initialized states in fucnction initStates
 */
#define STATE_DEF(state_array, status_type, state_type, description, transitions, stateLevel) \
			state_array[status_type][state_type]->st_description = description; \
			state_array[status_type][state_type]->st_transitions = transitions; \
			state_array[status_type][state_type]->st_stateLevel = stateLevel;
/* Same for events description assignment */ 
#define EVENT_DEF(event_array, status_type, event_type, description) \
			event_array[status_type][event_type]->ev_description  = description;
					  
/** 
 * the server routine which: 
 *  * accepts socket connections,
 *  * outputs a status message
 *  * and closes the socket immediately after that
 */
THREAD_RETURN serveMonitoringRequests(void* param);

/**
 * a conveinience function for easily defining state transitions
 * @param s_type the status type of the state for which the transitions 
 * 		  are to be defined
 * @param transitionCount the number of transitions to define
 * @param ... an event_type (of type event_type_t) followed by a 
 * 		  state transition (of type transition_t) 
 * 		  IMPORTANT: an event type MUST be followed by a state transition!
 * @return pointer to the array where the transitions are stored, (which of course 
 * 			has to be disposed by delete[] when reference is not needed anymore)!
 */
transition_t *defineTransitions(status_type_t s_type, SINT32 transitionCount, ...);

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
			ev_net_keyExchangeNextSuccessful, st_net_firstMixOnline, \
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
	(defineTransitions(stat_networking, 2, \
			ev_net_nextConnected, st_net_middleMixConnectedToNext, \
			ev_net_prevConnectionClosed, st_net_middleMixInit))

/* transitions for st_net_middleMixConnectedToNext: */
#define TRANS_NET_MIDDLE_MIX_CONNECTED_TO_NEXT \
	(defineTransitions(stat_networking, 4, \
			ev_net_keyExchangeNextFailed, st_net_middleMixInit, \
			ev_net_keyExchangePrevFailed, st_net_middleMixInit, \
			ev_net_keyExchangeNextSuccessful, st_net_middleMixConnectedToNext, \
			ev_net_keyExchangePrevSuccessful, st_net_middleMixOnline))

/* transitions for st_net_middleMixOnline: */
#define TRANS_NET_MIDDLE_MIX_ONLINE \
	(defineTransitions(stat_networking, 2, \
			ev_net_prevConnectionClosed, st_net_middleMixInit, \
			ev_net_nextConnectionClosed, st_net_middleMixInit))
			
/* transitions for st_net_lastMixInit: */
#define TRANS_NET_LAST_MIX_INIT \
	(defineTransitions(stat_networking, 1, \
			ev_net_prevConnected, st_net_lastMixConnectedToPrev))

/* transitions for st_net_lastMixConnectedToPrev: */
#define TRANS_NET_LAST_MIX_CONNECTED_TO_PREV \
	(defineTransitions(stat_networking, 2, \
			ev_net_keyExchangePrevFailed, st_net_lastMixInit, \
			ev_net_keyExchangePrevSuccessful, st_net_lastMixOnline))
			
			
/* transitions for st_net_lastMixOnline: */
#define TRANS_NET_LAST_MIX_ONLINE \
	(defineTransitions(stat_networking, 1, \
			ev_net_prevConnectionClosed, st_net_lastMixInit))

#ifdef PAYMENT
#define TRANS_PAY_DUMMY \
	(defineTransitions(stat_payment, 0))
#endif

#define TRANS_SYS_DUMMY \
	(defineTransitions(stat_system, 0))

class CAStatusManager
{

public:
	
	static void init();
	static void cleanup();
	/* Function to be called when an event occured, 
	 * may cause changing of current state 
	 */
	static SINT32 fireEvent(event_type_t e_type, status_type_t s_type);

private:
	/* current states for each status type */
	state_t **m_pCurrentStates;
	/* Keeps references to the nodes of the DOM tree
	 *  where the current Status informations are set 
	 */
	dom_state_info_t *m_pCurrentStatesInfo;
	/* synchronized access to all fields */  
	CAMutex *m_pStatusLock;
	/* ServerSocket to listen for monitoring requests */ 
	CASocket *m_pStatusSocket;
	/* Thread to answer monitoring requests */
	CAThread *m_pMonitoringThread;
	/* the DOM structure to create the XML status message */
	XERCES_CPP_NAMESPACE::DOMDocument* m_pPreparedStatusMessage; 
	
	/* StatusManger singleton */
	static CAStatusManager *ms_pStatusManager; 
	/* holds references all defined states 
	 * accessed by [status_type][state_type] 
	 */
	static state_t ***ms_pAllStates; 
	/* holds references all defined events 
	 * accessed by [status_type][event_type] 
	 */
	static event_t ***ms_pAllEvents;  
	
	CAStatusManager();
	virtual ~CAStatusManager();
	
	SINT32 initSocket();
	
	static void initStates();
	static void initEvents();
	
	static void deleteStates();
	static void deleteEvents();
	
	/* changes state  (only called by fireEvent) */
	SINT32 transition(event_type_t e_type, status_type_t s_type);
	SINT32 initStatusMessage();
	
	/* monitoring server routine */
	friend THREAD_RETURN serveMonitoringRequests(void* param);		
};
#endif

#endif /*CASTATUSMANAGER_HPP_*/
