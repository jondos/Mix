#ifndef CASTATUSMANAGER_HPP_
#define CASTATUSMANAGER_HPP_

#include "StdAfx.h"

/* How many different status types do exist*/
#define NR_STATUS_TYPES 3

/* How many different events do exist for each status type*/
#define NR_NETWORKING_EVENTS 7
#define NR_PAYMENT_EVENTS 1
#define NR_SYSTEM_EVENTS 1

#define NR_NETWORKING_STATES 8
#define NR_PAYMENT_STATES 1
#define NR_SYSTEM_STATES 1

static const UINT32 EVENT_NR[NR_STATUS_TYPES] =
{
		NR_NETWORKING_EVENTS, NR_PAYMENT_EVENTS, NR_SYSTEM_EVENTS
};
                       
enum state_type
{
	st_ignore = -1,
	/* networking states */
	st_net_entry,
	st_net_firstMixInit, st_net_firstMixOnline , 
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

struct event
{
	enum event_type ev_type;
	enum status_type ev_statusType;
};

typedef struct event event_t;

struct state
{
	enum state_type st_type;
	enum status_type st_statusType;
	char st_description[50];
	struct event *st_cause;
	enum state_type *st_transitions;
};

typedef struct state state_t;


class CAStatusManager
{
public:
	CAStatusManager();
	virtual ~CAStatusManager();
	void transition(enum event_type e_type, enum status_type s_type);
private:
	state_t *m_pCurrent_net_state;
	state_t *m_pCurrent_pay_state;
	state_t *m_pCurrent_sys_state;
};

#endif /*CASTATUSMANAGER_HPP_*/
