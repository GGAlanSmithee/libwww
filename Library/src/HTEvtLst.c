/*				    				     HTEvntrg.c
**	EVENT MANAGER
**
**	(c) COPYRIGHT MIT 1995.
**	Please first read the full copyright statement in the file COPYRIGH.
**	@(#) $Id$
**
**	Updated HTEvent module 
**	This new module combines the functions of the old HTEvent module and 
**	the HTThread module. We retain the old HTThread module, but it
**	consists of calls to the HTEvent interfaces
**
** Authors:
**	HFN	Henrik Frystyk <frystyk@w3.org>
**	CLB    	Charlie Brooks <cbrooks@osf.org>
** Bugs
**
*/

/*   WSAAsyncSelect and windows app stuff need the following definitions:
 *   WWW_WIN_ASYNC - enable WSAAsyncSelect instead of select
 *   _WIN23 - win32 libararies - may be window or console app
 *   _WINSOCKAPI_ - using WINSOCK.DLL - not necessarily the async routines.
 *   _CONSOLE - the console app for NT
 *
 * first pass: EGP - 10/26/95
 */

/* Implementation dependent include files */
#include "sysdep.h"
#include "WWWUtil.h"
#include "WWWCore.h"
#include "HTReqMan.h"
#include "HTTimer.h"
#include "HTEvtLst.h"					 /* Implemented here */

/* Type definitions and global variables etc. local to this module */
#define PRIME_TABLE_SIZE	67
#define MILLI_PER_SECOND	1000
#define HASH(s)			((s) % PRIME_TABLE_SIZE) 
#define HT_EVENT_ORDER				  /* use event ordering code */
#define EVENTS_TO_EXECUTE	5  /* how many to execute in one select loop */

#ifdef WWW_WIN_ASYNC
#define TIMEOUT	1 /* WM_TIMER id */
PRIVATE HWND HTSocketWin;
PRIVATE unsigned long HTwinMsg;
#else /* WWW_WIN_ASYNC */
PRIVATE fd_set FdArray[HTEvent_TYPES];
PRIVATE SOCKET MaxSock = 0 ;			  /* max socket value in use */
#endif /* !WWW_WIN_ASYNC */

typedef struct {
    SOCKET 	s ;	 		/* our socket */
    HTEvent * 	events[HTEvent_TYPES];	/* event parameters for read, write, oob */
    HTTimer *   timeouts[HTEvent_TYPES];/* timeout for each of the events */
} SockEvents;

typedef enum {
    SockEvents_mayCreate,
    SockEvents_find
} SockEvents_action;

HTList * HashTable[PRIME_TABLE_SIZE]; 
PRIVATE int HTEndLoop = 0;		       /* If !0 then exit event loop */

/* ------------------------------------------------------------------------- */

PRIVATE SockEvents * SockEvents_get (SOCKET s, SockEvents_action action)
{
    long v = HASH(s);
    HTList* cur;
    SockEvents * pres;

    if (HashTable[v] == NULL)
	HashTable[v] = HTList_new();
    cur = HashTable[v];
    while ((pres = (SockEvents *) HTList_nextObject(cur)))
	if (pres->s == s)
	    return pres;

    if (action == SockEvents_mayCreate) {
        if ((pres = (SockEvents *) HT_CALLOC(1, sizeof(SockEvents))) == NULL)
	    HT_OUTOFMEM("HTEventList_register");
	pres->s = s;
	HTList_addObject(HashTable[v], (void *)pres);
	return pres;
    }
    return NULL;
}

PUBLIC void HTEvent_traceHead(void)
{
    HTTrace("     event: pri millis  callback   param    request  ");
}
PUBLIC void HTEvent_trace(HTEvent * event)
{
    if (event == NULL)
	return;
    HTTrace("%8p: %3d %6d %8p %8p %8p", event, event->priority, event->millis, event->cbf, event->param, event->request);
}
PUBLIC void HTTimer_traceHead(void)
{
    HTTrace("     timer: millis expires ?   param   callback  ");
}
PRIVATE char * MyTime(unsigned long int time, int len)
{
static char space[100];
    sprintf(space, "1234567");
    return space;
}
struct _HTTimer {
    HTTimer *	next;		/* The next guy in line */
    ms_t	millis;		/* Relative value in millis */
    ms_t	expires;	/* Absolute value in millis */
    BOOL	relative;
    void *	param;		/* Client supplied context */
    HTTimerCallback * cbf;
};

PUBLIC void HTTimer_trace(HTTimer * timer)
{
    if (timer == NULL)
	return;
    HTTrace("%8p: %6d %7s %c %8p %8p", timer, timer->millis, MyTime(timer->expires, 7), 
	    timer->relative == YES ? 'R' : 'A', timer->param, timer->cbf);
}
/*
**  A simple debug function that dumps all the socket arrays
**  as trace messages
*/
PRIVATE void EventList_dump (void)
{
    long v;
    HTList* cur;
    SockEvents * pres;

    if (HashTable[v] == NULL) {
	HTTrace("Event....... No sockets registered\n");
	return;
    }
    HTTrace("Event....... Dumping socket events\n");
    HTTrace("soc ");
    HTEvent_traceHead();
    HTTrace(" ");
    HTTimer_traceHead();
    HTTrace("\n");
    for (v = 0; v < PRIME_TABLE_SIZE; v++) {
	cur = HashTable[v];
	while ((pres = (SockEvents *) HTList_nextObject(cur))) {
	    int i;
	    HTTrace("%3d \n", pres->s);
	    for (i = 0; i < HTEvent_TYPES; i++)
		if (pres->events[i]) {
		    static char * names[HTEvent_TYPES] = {"read", "writ", "xcpt"};
		    HTTrace("%s", names[i]);
		    HTEvent_trace(pres->events[i]);
		    HTTrace(" ");
		    HTTimer_trace(pres->timeouts[i]);
		    HTTrace(" ");
		}
	    HTTrace("\n");
	}
    }
}

/* ------------------------------------------------------------------------- */
/*		E V E N T   O R D E R I N G   S T U F F			     */
#ifdef HT_EVENT_ORDER
typedef struct {
    HTEvent *	event;
    SOCKET	s;
    HTEventType	type;
    HTPriority	skipped;
} EventOrder;

HTList * EventOrderList = NULL;
#if 0
/*
**	return -1 if a should be after b
 */
int EventOrderComparer (const void * a, const void * b)
{
    EventOrder * placeMe = (EventOrder *)a;
    EventOrder * maybeHere = (EventOrder *)b;
    if (placeMe->event->priority+placeMe->skipped >= maybeHere->event->priority+maybeHere->skipped)
	return 1;
    return -1;
}
#endif

int EventOrder_add (SOCKET s, HTEventType type)
{
    EventOrder * pres;
    HTList * cur = EventOrderList;
    HTList * insertAfter = cur;
    HTEvent * event = HTEventList_lookup(s, type);

    if (event == NULL) {
	HTTrace("EventOrder.. no event found for socket %d, type %x.\n", s, type);
	return HT_ERROR;
    }

    /*
    **	Look to see if it's already here from before
    */
    while ((pres = (EventOrder *) HTList_nextObject(cur))) {
	if (pres->s == s && pres->event == event && pres->type == type) {
	    pres->skipped++;
	    return HT_OK;
	}
	if (pres->event->priority+pres->skipped > event->priority)
	    insertAfter = cur;
    }

    /*
    **	No, so create a new element
    */
    if ((pres = (EventOrder *) HT_CALLOC(1, sizeof(EventOrder))) == NULL)
	HT_OUTOFMEM("EventOrder_add");
    pres->event = event;
    pres->s = s;
    pres->type = type;
    HTList_addObject(insertAfter, (void *)pres);
    return HT_OK;
}

PUBLIC int EventOrder_executeAndDelete (void) 
{
    HTList * cur;
    EventOrder * pres;
    int i = 0;

    if (cur == NULL)
	return NO;
#if 0
    HTList_insertionSort(EventOrderList, EventOrderComparer);
#endif
    cur = EventOrderList;
    if (THD_TRACE) HTTrace("EventOrder.. execute ordered events\n");
    while ((pres = (EventOrder *) HTList_removeLastObject(cur)) && i < EVENTS_TO_EXECUTE) {
	int ret = (*pres->event->cbf)(pres->s, pres->event->param, pres->type);
	HT_FREE(pres);
	if (ret != HT_OK)
	    return ret;
	i++;
    }
    return HT_OK;
}

PUBLIC BOOL EventOrder_deleteAll (void) 
{
    HTList * cur = EventOrderList;
    EventOrder * pres;

    if (cur == NULL)
	return NO;
    if (THD_TRACE) HTTrace("EventOrder.. all ordered events\n");
    while ((pres = (EventOrder *) HTList_nextObject(cur)))
	HT_FREE(pres);
    HTList_delete(EventOrderList);
    EventOrderList = NULL;
    return YES;
}
#endif /* HT_EVENT_ORDER */

/* ------------------------------------------------------------------------- */
/*		T I M E O U T   H A N D L E R				     */
PRIVATE int EventListTimerHandler (HTTimer * timer, void * param)
{
    SockEvents * sockp = (SockEvents *)param;
    HTEvent * event;
    if (sockp->timeouts[HTEvent_INDEX(HTEvent_READ)] == timer) {
	event = sockp->events[HTEvent_INDEX(HTEvent_READ)];
	if (THD_TRACE) HTTrace("Event....... READ timed out on %d.\n", sockp->s);
	return (*event->cbf) (sockp->s, event->param, HTEvent_TIMEOUT);
    }
    if (sockp->timeouts[HTEvent_INDEX(HTEvent_WRITE)] == timer) {
	event = sockp->events[HTEvent_INDEX(HTEvent_WRITE)];
	if (THD_TRACE) HTTrace("Event....... WRITE timed out on %d.\n", sockp->s);
	return (*event->cbf) (sockp->s, event->param, HTEvent_TIMEOUT);
    }
    if (sockp->timeouts[HTEvent_INDEX(HTEvent_OOB)] == timer) {
	event = sockp->events[HTEvent_INDEX(HTEvent_OOB)];
	if (THD_TRACE) HTTrace("Event....... OOB timed out on %d.\n", sockp->s);
	return (*event->cbf) (sockp->s, event->param, HTEvent_TIMEOUT);
    }
    HTTrace("Event....... can't find event for timer %p.\n", timer);
    return HT_ERROR;
}

/* ------------------------------------------------------------------------- */

/*
**  For a given socket, reqister a request structure, a set of operations, 
**  a HTEventCallback function, and a priority. For this implementation, 
**  we allow only a single HTEventCallback function for all operations.
**  and the priority field is ignored.
*/
PUBLIC int HTEventList_register (SOCKET s, HTEventType type, HTEvent * event)
{
    SockEvents * sockp;
    if (THD_TRACE) 
	HTTrace("Event....... Register socket %d, request %p handler %p type %x at priority %d\n",
		s, (void *) event->request,
		(void *) event->cbf, (unsigned) type,
		(unsigned) event->priority);
    if (s==INVSOC || HTEvent_INDEX(type) > 2)
	return 0;
#if 0
    /*
    ** Don't write down TIMEOUT events in the SockEvents list or the fd sets.
    ** They just manifest in the HTTimer
    */
    if (type == HTEvent_TIMEOUT)
	return HT_OK;
#endif /* 0 */

    /*
    ** Insert socket into appropriate file descriptor set. We also make sure
    ** that it is registered in the global set.
    */
    if (THD_TRACE) HTTrace("Event....... Registering socket for %d\n", type);
    sockp = SockEvents_get(s, SockEvents_mayCreate);
    sockp->s = s;
    sockp->events[HTEvent_INDEX(type)] = event;
#ifdef WWW_WIN_ASYNC
    if (WSAAsyncSelect(s, HTSocketWin, HTwinMsg, HTEvent_INDEX(type)) < 0)
	return HT_ERROR;
#else /* WWW_WIN_ASYNC */
    FD_SET(s, FdArray+HTEvent_INDEX(type));
    if (s > MaxSock) MaxSock = s ;
#endif /* !WWW_WIN_ASYNC */
    /*
    ** If the timeout has been set (relative in millis) then we register 
    ** a new timeout for this event
    */
    if (event->millis >= 0) {
	sockp->timeouts[HTEvent_INDEX(type)] = HTTimer_new(NULL, EventListTimerHandler, sockp, event->millis, YES);
    }

    return HT_OK;
}

PRIVATE int EventList_remaining(SockEvents * pres)
{
    int ret = 0;
    int i;
    for (i = 0; i < HTEvent_TYPES; i++)
	if (pres->events[i] != NULL)
	    ret |= 1<<i;
    return ret;
}

/*
** Remove the registered information for the specified socket for the actions 
** specified in ops. if no actions remain after the unregister, the registered
** info is deleted, and, if the socket has been registered for notification, 
** the HTEventCallback will be invoked.
*/
PUBLIC int HTEventList_unregister(SOCKET s, HTEventType type) 
{
    long 		v = HASH(s);
    HTList * 		cur = HashTable[v];
    HTList * 		last = cur;
    SockEvents *	pres;

    while ((pres = (SockEvents *) HTList_nextObject(cur))) {
        if (pres->s == s) {
	    int		remaining;

	    /*
	    **  Unregister the event from this action
	    */
	    pres->events[HTEvent_INDEX(type)] = NULL;

	    /*
	    **  Check to see of there was a timeout connected with the event.
	    **  If so then delete the timeout as well.
	    */
	    {
		HTTimer * timer = pres->timeouts[HTEvent_INDEX(type)];
		if (timer) HTTimer_delete(timer);
	    }
	    
	    /*
	    **  Check to see if we can delete the action completely. We do this
	    **  if there are no more events registered.
	    */
	    if ((remaining = EventList_remaining(pres)) == 0) {
		if (THD_TRACE)
		    HTTrace("Event....... No more events registered for socket %d\n", s);
		HT_FREE(pres);
		HTList_quickRemoveElement(cur, last);
	    } else
		last = cur;  /* to set next pointer when creating new */
#ifdef WWW_WIN_ASYNC
	    if (WSAAsyncSelect(s, HTSocketWin, HTwinMsg, remaining) < 0)
		return HT_ERROR;
#else /* WWW_WIN_ASYNC */
	    FD_CLR(s, FdArray+HTEvent_INDEX(type));
#endif /* !WWW_WIN_ASYNC */
      	    if (THD_TRACE)
		HTTrace("Event....... Socket %d unregisterd for %x\n", s, type);
	    return HT_OK;
	}
    }
    if (THD_TRACE) HTTrace("Event....... Couldn't find socket %d.\n", s);
    return HT_ERROR;
}

/*
** Unregister all sockets 
** N.B. we just remove them for our internal data structures: it is up to the 
** application to actually close the socket. 
*/
PUBLIC int HTEventList_unregisterAll (void) 
{
    int i;
    if (THD_TRACE) HTTrace("Unregister.. all sockets\n");
    for (i = 0 ; i < PRIME_TABLE_SIZE; i++) {
	HTList * cur = HashTable[i];
	SockEvents * pres;
	while ((pres = (SockEvents *) HTList_nextObject(cur))) {
#ifdef WWW_WIN_ASYNC
	    WSAAsyncSelect(pres->s, HTSocketWin, HTwinMsg, 0);
#endif /* WWW_WIN_ASYNC */
	    HT_FREE(pres);
	}
	HTList_delete(HashTable[i]);
	HashTable[i] = NULL;
    }
#ifndef WWW_WIN_ASYNC
    MaxSock = 0 ;
    FD_ZERO(FdArray+HTEvent_INDEX(HTEvent_READ));
    FD_ZERO(FdArray+HTEvent_INDEX(HTEvent_WRITE));
    FD_ZERO(FdArray+HTEvent_INDEX(HTEvent_OOB));
#endif /* !WWW_WIN_ASYNC */
#ifdef HT_EVENT_ORDER
    EventOrder_deleteAll();
#endif /* HT_EVENT_ORDER */
    return 0;
}

/*
**  Dispatch the event to the appropriate event handler.
**  If no event handler is found then just return.
*/
PUBLIC int HTEventList_dispatch (SOCKET s, HTEventType type)
{
    SockEvents * sockp = SockEvents_get(s, SockEvents_find);
    if (sockp) {
	HTEvent * event = sockp->events[HTEvent_INDEX(type)];

	/*
	**  If we have found an event object for this event then see
	**  is we should call it.
	*/
	if (event && event->priority!=HT_PRIORITY_OFF)
	    return (*event->cbf) (s, event->param, type);
	if (THD_TRACE) HTTrace("Dispatch.... Handler %p NOT called\n", sockp);
	return HT_OK;
    }
    if (THD_TRACE) HTTrace("Dispatch.... Bad socket %d\n", s);
    return NO;
}

/*
**  Stops the (select based) event loop. The function does not guarantee
**  that all requests have terminated. This is for the app to do
*/
PUBLIC void HTEventList_stopLoop (void)
{
    HTEndLoop = 1;
}

PUBLIC HTEvent * HTEventList_lookup (SOCKET s, HTEventType type)
{
    SockEvents * sockp = NULL;
    if ((sockp = SockEvents_get(s, SockEvents_find)) == NULL)
	return NULL;
    return sockp->events[HTEvent_INDEX(type)];
}

/*	REGISTER DEFULT EVENT MANAGER
**	-----------------------------
**	Not done automaticly - may be done by application!
*/
PUBLIC BOOL HTEventInit (void)
{
    HTEvent_setRegisterCallback(HTEventList_register);
    HTEvent_setUnregisterCallback(HTEventList_unregister);
    return YES;
}

PUBLIC BOOL HTEventTerminate (void)
{
    return YES;
}

#ifdef WWW_WIN_ASYNC

/*	HTEventrg_get/setWinHandle
**	--------------------------
**	Managing the windows handle on Windows
*/
PUBLIC BOOL HTEventList_setWinHandle (HWND window, unsigned long message)
{
    HTSocketWin = window;
    HTwinMsg = message;
    return YES;
}

PUBLIC HWND HTEventList_getWinHandle (unsigned long * pMessage)
{
    if (pMessage)
        *pMessage = HTwinMsg;
    return (HTSocketWin);
}

PUBLIC BOOL HTEventList_asyncWindowsTimer(HTTimer * timer, ms_t millis)
{
    if (SetTimer(HTSocketWin, (UINT)timer, millis) > 0)
	return YES;
    return NO;
}

/* only responsible for WM_TIMER and WSA_AsyncSelect */    	
PUBLIC LRESULT CALLBACK AsyncWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WORD event;
    SOCKET sock;

    /* timeout stuff */
    if (uMsg == WM_TIMER) {
	HTTimer_dispatch((HTTimer *)lParam);
	return (0);
    }

    if (uMsg != HTwinMsg)	/* not our async message */
    	return (DefWindowProc(hwnd, uMsg, wParam, lParam));

    event = LOWORD(lParam);
    sock = (SOCKET)wParam;
    if (event & (FD_READ | FD_ACCEPT | FD_CLOSE))
    	if (HTEventrg_dispatch((int)sock, FD_READ) != HT_OK) {
	    HTEndLoop = -1;
	    return 0;
	}
    if (event & (FD_WRITE | FD_CONNECT))
    	if (HTEventrg_dispatch((int)sock, FD_WRITE) != HT_OK) {
	    HTEndLoop = -1;
	    return 0;
	}
    if (event & FD_OOB)
    	if (HTEventrg_dispatch((int)sock, FD_OOB) != HT_OK) {
	    HTEndLoop = -1;
	    return 0;
	}
    return (0);
}

PUBLIC int HTEventrg_loop (HTRequest * theRequest )
{
    MSG msg;
    while (GetMessage(&msg,0,0,0)) {
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
    }
    return (HTEndLoop == 1 ? HT_OK : HT_ERROR);
}

#else /* WWW_WIN_ASYNC */

/*
**  We wait for activity from one of our registered 
**  channels, and dispatch on that.
**
**  There are now two versions of the event loop. The first is if you want
**  to use async I/O on windows, and the other is if you want to use normal
**  Unix setup with sockets
*/
PUBLIC int HTEventList_loop (HTRequest * theRequest) 
{
    fd_set treadset, twriteset, texceptset;
    struct timeval waittime, * wt;
    int active_sockets;
    int maxfds;
    int timeout;
    SOCKET s;
    int status = 0;
    HTEndLoop = 0;

    EventOrderList = HTList_new();	/* is kept around until EventOrder_deleteAll */

    /* Don't leave this loop until we leave the application */
    do {
        treadset = FdArray[HTEvent_INDEX(HTEvent_READ)];
        twriteset = FdArray[HTEvent_INDEX(HTEvent_WRITE)];
        texceptset = FdArray[HTEvent_INDEX(HTEvent_OOB)];
        maxfds = MaxSock; 

	if (THD_TRACE) HTTrace("Event Loop.. calling select: maxfds is %d\n", maxfds);

        /*
	**  Timeval struct copy needed for linux, as it set the value to the
	**  remaining timeout while exiting the select. (and perhaps for
	**  other OS). Code borrowed from X server.
	*/
	wt = NULL;
	if ((timeout = HTTimer_soonest())) {
	    waittime.tv_sec = timeout / MILLI_PER_SECOND;
	    waittime.tv_usec = (timeout % MILLI_PER_SECOND) *
		(1000000 / MILLI_PER_SECOND);
	    wt = &waittime;
	}

	HTTraceData((char*)&treadset, maxfds/8 + 1, "HTEventList_loop pre treadset: (maxfd:%d)", maxfds);
	HTTraceData((char*)&twriteset, maxfds/8 + 1, "HTEventList_loop pre twriteset:");
	HTTraceData((char*)&texceptset, maxfds/8 + 1, "HTEventList_loop pre texceptset:");

#ifdef __hpux 
        active_sockets = select(maxfds+1, (int *)&treadset, (int *)&twriteset,
				(int *)&texceptset, wt);
#else
        active_sockets = select(maxfds+1, &treadset, &twriteset, &texceptset, wt);
#endif

	HTTraceData((char*)&treadset, maxfds/8 + 1, "HTEventList_loop post treadset: (active_sockets:%d)", active_sockets);
	HTTraceData((char*)&twriteset, maxfds/8 + 1, "HTEventList_loop post twriteset: (errno:%d)", errno);
	HTTraceData((char*)&texceptset, maxfds/8 + 1, "HTEventList_loop post texceptset:");

	if (THD_TRACE) HTTrace("Event Loop.. select returns %d\n", active_sockets);

        if (active_sockets == -1) {
	    HTRequest_addSystemError( theRequest, ERR_FATAL, socerrno, NO, "select");
	    EventList_dump();
#if 0
	    __DumpFDSet(FdArray+HTEvent_INDEX(HTEvent_READ), "Read");
	    __DumpFDSet(FdArray+HTEvent_INDEX(HTEvent_WRITE), "Write") ;
	    __DumpFDSet(FdArray+HTEvent_INDEX(HTEvent_OOB), "Exceptions");
#endif /* 0 */
	    return HT_ERROR;
        }

	/*
	**  We had a timeout so now we check and see if we have a timeout
	**  handler to call
	*/ 
	if (active_sockets == 0) {
	    HTTimer_dispatchAll();
	    continue;
	}

	/*
	**  There were active sockets. Determine which fd sets they were in
	*/
#ifdef HT_EVENT_ORDER
#define DISPATCH(socket, type) EventOrder_add(socket, type)
#else /* HT_EVENT_ORDER */
#define DISPATCH(socket, type) HTEventList_dispatch(socket, type)
#endif /* !HT_EVENT_ORDER */
	for (s = 0 ; s <= maxfds ; s++) { 
	    if (FD_ISSET(s, &texceptset))
		if ((status = DISPATCH(s, HTEvent_OOB)) != HT_OK)
		    return status;
	    if (FD_ISSET(s, &twriteset))
		if ((status = DISPATCH(s, HTEvent_WRITE)) != HT_OK)
		    return status;
	    if (FD_ISSET(s, &treadset))
		if ((status = DISPATCH(s, HTEvent_READ)) != HT_OK)
		    return status;
	}
#ifdef HT_EVENT_ORDER
	if ((status = EventOrder_executeAndDelete()) != HT_OK)
	    return status;
#endif /* HT_EVENT_ORDER */
    } while (!HTEndLoop);

    return HT_OK;
}

#endif /* !WWW_WIN_ASYNC */