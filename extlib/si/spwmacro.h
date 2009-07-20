/*----------------------------------------------------------------------
 *  spwmacro.h -- cpp macros we ALWAYS use.
 *
<<<<<<< spwmacro.h
 *  $Id: spwmacro.h,v 1.3 2001/01/16 01:18:40 HJin Exp $
=======
 *  $Id: spwmacro.h,v 1.3 2001/01/16 01:18:40 HJin Exp $
>>>>>>> 1.1.1.1.4.1
 *
 *   We always seem to use the same macros.
 *   This is the place we define them.
 *
 *----------------------------------------------------------------------
 */

#ifndef SPWMACRO_H
#define SPWMACRO_H

 
#define SPW_FALSE   (0)
#define SPW_TRUE    (!SPW_FALSE)

#define SPW_MAX(a,b)   (((a)>(b))?(a):(b))
#define SPW_MIN(a,b)   (((a)<(b))?(a):(b))

#define SPW_ABS(a)   (((a)<0)?(-(a)):(a))

#define SPW_SIGN(a)  ((a)>=0?1:-1)

#define SPW_BIND(min,n,max)   (SPW_MIN((max),SPW_MAX((min),(n))))

#define SPW_NUM_ELEMENTS_IN(a)   (sizeof(a)/sizeof((a)[0]))

#define SPW_PI   3.14159265358979324f

#define SPW_DEG_TO_RAD(d)   ((d)*SPW_PI/180.0f)
#define SPW_RAD_TO_DEG(r)   ((r)*180.0f/SPW_PI)

#define SPW_LENGTH_OF(a)   (sizeof(a)/sizeof((a)[0]))

#define SPW_END_OF(a)   (&(a)[SPW_LENGTH_OF(a)-1])

#define SPW_SQ(a)   ((a)*(a))

#define SPW_ABSDIFF(a, b) (fabs((double) (a) - (b)))


#endif
