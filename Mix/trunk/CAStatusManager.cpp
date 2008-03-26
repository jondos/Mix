#include "CAStatusManager.hpp"
#include "CAMsg.hpp"

/** transitions for the defined states **/

/* state transition when the corresponing events from enumeration occurs, the order is the same as above:
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
	st_ignore, st_net_firstMixOnline, 
	st_ignore, st_ignore
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
		{st_net_entry, stat_networking, "networking entry state", NULL, trans_net_entry},
		{st_net_firstMixInit, stat_networking, "", NULL, trans_net_firstMixInit},
		{st_net_firstMixOnline, stat_networking, "", NULL, trans_net_firstMixOnline},
		{st_net_middleMixInit, stat_networking, "", NULL, trans_net_middleMixInit},
		{st_net_middleMixConnectedToPrev, stat_networking, "", NULL, trans_net_middleMixConnectedToPrev},
		{st_net_middleMixOnline, stat_networking, "", NULL, trans_net_middleMixOnline},
		{st_net_lastMixInit, stat_networking, "", NULL, trans_net_lastMixInit}, 
		{st_net_lastMixOnline, stat_networking, "", NULL, trans_net_lastMixOnline}
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

CAStatusManager::CAStatusManager()
{
	m_pCurrent_net_state = &(all_states[stat_networking][st_net_entry]);
	m_pCurrent_pay_state = &(all_states[stat_payment][st_pay_entry]);
	m_pCurrent_sys_state = &(all_states[stat_system][st_sys_entry]);
	CAMsg::printMsg(LOG_DEBUG, "Init states: net - %s, payment - %s, system - %s\n",
			m_pCurrent_net_state->st_description,
			m_pCurrent_pay_state->st_description,
			m_pCurrent_sys_state->st_description);
}

CAStatusManager::~CAStatusManager()
{
}

void CAStatusManager::transition(enum event_type e_type, enum status_type s_type)
{
	
}
