/* util_templates.h
**
**	Defines some widely used templates. This is basically a substitute
** for macros defined in util_macros.h
*/

#ifndef __UTIL_TEMPLATES_H
#define __UTIL_TEMPLATES_H

#ifndef __cplusplus
#   error		Must use C++ for templates.
#endif

#undef min		// In case those were defined...
#undef max
#undef diff
#undef member

template <class MinVal>
inline MinVal min (MinVal a, MinVal b)
{   
    return (a < b ? a : b);
}      

template <class MaxVal>
inline MaxVal max (MaxVal a, MaxVal b)
{   
    return (a > b ? a : b);
}      

template <class DiffVal>
inline DiffVal diff (DiffVal a, DiffVal b)
{
    return (a > b ? (a - b) : (b - a));
}

template <class RandLimit>
inline RandLimit randnum (RandLimit range)
{
static WORD seed = 736843782;
const WORD b = 31415821;
const WORD m = 100000000;
const WORD m1 = 10000;
int p1, p0, q1, q0;

    if (range != 0) {
       p1 = seed / m1;
       p0 = seed % m1;
       q1 = b / m1;
       q0 = b % m1;
       seed = (((((p0*q1 + p1*q0) % m1) * m1 + p0 * q0) % m) + 1) % m;
       return (seed % range);
    }
    else
       return (0);
}

template <class Num>
inline int sign (Num x)
{
    return (x < 0 ? -1 : 1);
}

template <class ArrayEl>
inline BOOL member (ArrayEl el, ArrayEl * arr, size_t size)
{   
size_t n;	  
    for (n = 0; n < size; ++ n)
       if (arr[n] == el)
          return (TRUE);
    return (FALSE);	   
}

#endif

