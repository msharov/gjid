/* stack.h
**
**	An out-of-the-book stack class. Very trivial. Just Push and Pop. 
**
** includes:
**	chain.h	- for Chain base class
*/

#ifndef __STACK_H
#define __STACK_H

#include <chain.h>

template <class StackEl>
class Stack : public Chain<StackEl> {
public:
    virtual inline void		Push (StackEl * element);
    virtual inline StackEl *	Pop (void);
    inline StackEl *		Top (void);
};

////////////////////////////////////////////////////////////////////////////
//////////////////////////// Functions /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

template <class StackEl>
inline void Stack<StackEl> :: Push 
(StackEl * element)
{
ChainLink<StackEl> * NewLink;
 
    NewLink = new ChainLink<StackEl>;
    NewLink->SetData (element);
    ConnectBefore (MoveToHead(), NewLink);
}

template <class StackEl>
inline StackEl * Stack<StackEl> :: Pop (void)
{
ChainLink<StackEl> * ptr;
StackEl * data;

    if (IsEmpty())
    {
       cout << "Stack: Popping from an empty stack!\n";
       return (NULL);
    }

    ptr = MoveToHead();
    Disconnect (ptr);
    data = ptr->GetData();
    delete ptr;
    return (data);
}

template <class StackEl>
inline StackEl * Stack<StackEl> :: Top (void)
{
ChainLink<StackEl> * ptr;
StackEl * data = NULL;

    ptr = MoveToHead();
    if (ptr != NULL)
       data = ptr->GetData();
    return (data);
}

#endif 

