/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    queue.h

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/

#include <ndis.h>
#include <efilter.h>

#include "equates.h"
#include "pci.h"
#include "82557.h"
#include "d100dbg.h"
#include "d100sw.h"


//-------------------------------------------------------------------------
// QueueInitList -- Macro which will initialize a queue to NULL.
//-------------------------------------------------------------------------
#define QueueInitList(_L) (_L)->Link.Flink = (_L)->Link.Blink = (PLIST_ENTRY)0;


//-------------------------------------------------------------------------
// QueueEmpty -- Macro which checks to see if a queue is empty.
//-------------------------------------------------------------------------
#define QueueEmpty(_L) (QueueGetHead((_L)) == (PD100_LIST_ENTRY)0)


//-------------------------------------------------------------------------
// QueueGetHead -- Macro which returns the head of the queue, but does not
// remove the head from the queue.
//-------------------------------------------------------------------------
#define QueueGetHead(_L) ((PD100_LIST_ENTRY)((_L)->Link.Flink))


//-------------------------------------------------------------------------
// QueuePushHead -- Macro which puts an element at the head of the queue.
//-------------------------------------------------------------------------
#define QueuePushHead(_L,_E) \
    ASSERT(_L); \
    ASSERT(_E); \
    if (!((_E)->Link.Flink = (_L)->Link.Flink)) \
    { \
        (_L)->Link.Blink = (PLIST_ENTRY)(_E); \
    } \
(_L)->Link.Flink = (PLIST_ENTRY)(_E);


//-------------------------------------------------------------------------
// QueueRemoveHead -- Macro which removes the head of the head of queue.
//-------------------------------------------------------------------------
#define QueueRemoveHead(_L) \
    {                                                     \
    PD100_LIST_ENTRY ListElem;                        \
    ASSERT((_L));                                     \
    if (ListElem = (PD100_LIST_ENTRY)(_L)->Link.Flink) /* then fix up our our list to point to next elem */ \
        {   \
            if(!((_L)->Link.Flink = ListElem->Link.Flink)) /* rechain list pointer to next link */ \
                /* if the list pointer is null, null out the reverse link */ \
                (_L)->Link.Blink = (PLIST_ENTRY) 0; \
        } }

//-------------------------------------------------------------------------
// QueuePutTail -- Macro which puts an element at the tail (end) of the queue.
//-------------------------------------------------------------------------
#define QueuePutTail(_L,_E) \
    ASSERT(_L); \
    ASSERT(_E); \
    if ((_L)->Link.Blink) \
    { \
        ((PD100_LIST_ENTRY)(_L)->Link.Blink)->Link.Flink = (PLIST_ENTRY)(_E); \
        (_L)->Link.Blink = (PLIST_ENTRY)(_E); \
    } \
    else \
    { \
        (_L)->Link.Flink = \
        (_L)->Link.Blink = (PLIST_ENTRY)(_E); \
    } \
(_E)->Link.Flink = (PLIST_ENTRY)0;

//-------------------------------------------------------------------------
// QueueGetTail -- Macro which returns the tail of the queue, but does not
// remove the tail from the queue.
//-------------------------------------------------------------------------
#define QueueGetTail(_L) ((PD100_LIST_ENTRY)((_L)->Link.Blink))

//-------------------------------------------------------------------------
// QueuePopHead -- Macro which  will pop the head off of a queue (list), and
//                 return it (this differs only from queueremovehead only in the 1st line)
//-------------------------------------------------------------------------
#define QueuePopHead(_L) \
(PD100_LIST_ENTRY) (_L)->Link.Flink; QueueRemoveHead(_L);


