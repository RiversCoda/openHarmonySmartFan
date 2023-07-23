#ifndef __DEBUG_H_
#define __DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>

#ifdef DEBG
#define DBG(...) printf("FILE:%s, FUNC:%s(), LINE:%d ",__FILE__,__FUNCTION__,__LINE__);printf(__VA_ARGS__)
#else
#define DBG(...)   
#endif

#ifdef __cplusplus
}
#endif

#endif
