/* queue.h
**
**	An out-of-the-book queue class. Very trivial. Just Append and Serve. 
**
** includes:
**	chain.h	- for Chain base class
*/

#ifndef __QUEUE_H
#define __QUEUE_H		      

#include <chain.h>

template <class QueueEl>
class Queue : public Chain<QueueEl> {
public:
    virtual inline void		Append (QueueEl * element);
    virtual inline QueueEl *	Serve (void);
    inline QueueEl *		Front (void);
};

////////////////////////////////////////////////////////////////////////////
//////////////////////////// Functions /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

template <class QueueEl>
inline void Queue<QueueEl> :: Append 
(QueueEl * element)
{
ChainLink<QueueEl> * NewLink;
 
    NewLink = new ChainLink<QueueEl>;
    NewLink->SetData (element);
    ConnectBefore (MoveToHead(), NewLink);
}

template <class QueueEl>
inline QueueEl * Queue<QueueEl> :: Serve (void)
{
ChainLink<QueueEl> * ptr;
QueueEl * data;

    if (IsEmpty())
    {
       cout << "Queue: Serving from an empty queue!\n";
       return (NULL);
    }

    ptr = MoveToTail();
    Disconnect (ptr);
    data = ptr->GetData();
    delete ptr;
    return (data);
}

template <class QueueEl>
inline QueueEl * Queue<QueueEl> :: Front (void)
{
ChainLink<QueueEl> * ptr;
QueueEl * data = NULL;

    ptr = MoveToTail();
    if (ptr != NULL)
       data = ptr->GetData();
    return (data);
}

#endif 

