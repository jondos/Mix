#include "StdAfx.h"
#include "CAThread.hpp"
#include "CAThreadList.hpp"
#include "CAMsg.hpp"

CAThreadList::CAThreadList()
{
	m_pHead = NULL;
	m_pListLock = new CAMutex();
}

CAThreadList::~CAThreadList()
{
	m_pListLock->lock();
	__removeAll();
	m_pListLock->unlock();
	
	delete m_pListLock;
}


/* safe functions */
UINT32 CAThreadList::put(CAThread *thread, pthread_t thread_id)
{
	UINT32 ret = 0;
	
	m_pListLock->lock();
	ret = __put(thread, thread_id);
	m_pListLock->unlock();
	
	return ret;
}

CAThread *CAThreadList::remove(pthread_t thread_id)
{
	CAThread *ret;
	
	m_pListLock->lock();
	ret = __remove(thread_id);
	m_pListLock->unlock();
		
	return ret;
}

CAThread *CAThreadList::get(CAThread *thread, pthread_t thread_id)
{
	CAThread *ret;
	
	m_pListLock->lock();
	ret = __get(thread_id);
	m_pListLock->unlock();
		
	return ret;
}

void CAThreadList::showAll()
{
	m_pListLock->lock();
	__showAll();
	m_pListLock->unlock();
}

UINT32 CAThreadList::getSize()
{
	UINT32 ret = 0;
	
	m_pListLock->lock();
	ret = __getSize();
	m_pListLock->unlock();
	
	return ret;
}

/* unsafe functions */
UINT32 CAThreadList::__put(CAThread *thread, pthread_t thread_id)
{
	thread_list_entry_t *new_entry = new thread_list_entry_t();
	//thread_list_entry_t *iterator;
	
	new_entry->tle_thread = thread;
	new_entry->tle_thread_id = thread_id;
	new_entry->tle_next = m_pHead;
	
	m_pHead = new_entry;	
	
	
	/*if(m_pHead == NULL)
	{
		CAMsg::printMsg(LOG_INFO, "CAThreadList::put - new Head inserted: Thread %s, id %x\n",
				thread->getName(), thread_id);
		m_pHead = new_entry;
		return ret;
	}*/
	
	
			
	/*for(iterator = m_pHead; iterator->tle_next != NULL; iterator = iterator->tle_next)
	{
		if(iterator->tle_thread_id == thread_id)
		{
			CAMsg::printMsg(LOG_INFO, 
					"CAThreadList::put - already inserted thread %s with id %x! Appending to the list nevertheless\n",
					thread->getName(), thread_id);
			ret = 1;
		}
	}*/
	
	//CAMsg::printMsg(LOG_INFO, "CAThreadList::put: Succesfully added Thread %s, id %x\n", 
	//							m_pHead->tle_thread->getName(), m_pHead->tle_thread_id);
	//iterator->tle_next = new_entry;
	return 0;
}

CAThread *CAThreadList::__remove(pthread_t thread_id)
{
	thread_list_entry_t *iterator;
	thread_list_entry_t *prev;
	CAThread *match = NULL;
	
	//CAMsg::printMsg(LOG_INFO, "CAThreadList::remove: try to remove thread with id %x\n", thread_id);
	
	if(m_pHead == NULL)
	{
		return NULL;
	}
	
	for(iterator = m_pHead, prev = NULL;
		iterator->tle_next != NULL; 
		prev = iterator, iterator = iterator->tle_next) 
	{
		//found a thread with the corresponding id
		if(iterator->tle_thread_id == thread_id)
		{
			
			match = iterator->tle_thread;
			//CAMsg::printMsg(LOG_INFO, "CAThreadList::remove: found Thread %s, id %x to remove\n",
			//		match->getName(), thread_id);
			if(prev != NULL)
			{
				//unchain
				prev->tle_next = iterator->tle_next;
			}
			else
			{
				//Head holds the corresponding thread;
				m_pHead = iterator->tle_next;
			}
			//dispose the entry
			iterator->tle_thread = NULL;
			iterator->tle_next = NULL;
			
			delete iterator;
			//CAMsg::printMsg(LOG_INFO, "CAThreadList::remove: Thread %s, id %x successfully removed\n",
			//					match->getName(), thread_id);
			return match;
		}
	}
	
	//CAMsg::printMsg(LOG_INFO, "CAThreadList::remove: no thread found with id %x\n", thread_id);
	
	return NULL;
}

CAThread *CAThreadList::__get(pthread_t thread_id)
{
	thread_list_entry_t *iterator;
	
	if(m_pHead == NULL)
	{
		return NULL;
	}
	
	for(iterator = m_pHead; iterator->tle_next != NULL; iterator = iterator->tle_next)
	{
		if(iterator->tle_thread_id == thread_id)
		{
			return iterator->tle_thread;
		}
	}
	return NULL;
}

void CAThreadList::__showAll()
{
	thread_list_entry_t *iterator;
	
	if(m_pHead == NULL)
	{
		CAMsg::printMsg(LOG_INFO, "CAThreadList::showAll: list is empty, no threads found\n");
	}
	
	for(iterator = m_pHead; iterator->tle_next != NULL; iterator = iterator->tle_next)
	{
		if(iterator->tle_thread!=NULL)
		{
			CAMsg::printMsg(LOG_INFO, "CAThreadList::showAll: Thread %s, id %x\n", 
									iterator->tle_thread->getName(),
									iterator->tle_thread_id);
		}
	}
}

// This only deletes the list entries not the corresponding threads!
void CAThreadList::__removeAll()
{
	thread_list_entry_t *iterator;
	
	if(m_pHead != NULL)
	{
		while(m_pHead->tle_next != NULL)
		{
			iterator = m_pHead->tle_next;
			m_pHead->tle_next = iterator->tle_next;
			
			CAMsg::printMsg(LOG_INFO, "CAThreadList::removeAll: Thread %s, id %x\n", 
										iterator->tle_thread->getName(),
										iterator->tle_thread_id);
			iterator->tle_thread = NULL;
			iterator->tle_next = NULL;
			delete iterator;
		}
		
		CAMsg::printMsg(LOG_INFO, "CAThreadList::removeAll: Thread %s, id %x\n", 
									m_pHead->tle_thread->getName(),
									m_pHead->tle_thread_id);
		m_pHead->tle_thread = NULL;
		delete m_pHead;
	}
	else 
	{
		CAMsg::printMsg(LOG_INFO, "CAThreadList::removeAll: list is already\n");
	}
}

UINT32 CAThreadList::__getSize()
{
	thread_list_entry_t *iterator;
	UINT32 count; 
		
	if(m_pHead == NULL)
	{
		return 0;
	}
		
	for(iterator = m_pHead, count = 0; 
		iterator->tle_next != NULL; 
		iterator = iterator->tle_next, count++);
	
	return count;
}
