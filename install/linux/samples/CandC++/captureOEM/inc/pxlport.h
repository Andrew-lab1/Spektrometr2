

#ifndef PIXELINK_COM_PXLPORT_H
#define PIXELINK_COM_PXLPORT_H

#include <PixeLINKApi.h>

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Assert
//
#ifndef ASSERT
#include <assert.h>
#define ASSERT(x)   do { assert((x)); } while(0)
#endif

////////////////////////////////////////////////////////////////////////////////
// Static Assert
//

#ifndef STATIC_ASSERT
#if defined(PIXELINK_LINUX)
#define STATIC_ASSERT(x, msg)   do { char static_assert_failure ## msg[(true == (x)) ? 1 : -1]; static_assert_failure ## msg; } while(0)
#else
#define STATIC_ASSERT(x, msg)   do { char static_assert_failure ## msg[(true == (x)) ? 1 :  0]; static_assert_failure ## msg; } while(0)
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
// Time 
//

PXL_RETURN_CODE	PxLPortInitialize();
PXL_RETURN_CODE PxLPortUninitialize();

////////////////////////////////////////////////////////////////////////////////
// Time 
//
typedef struct {
    int hours;
    int minutes;
    int seconds;
    int milliseconds;
} PXL_TIMESTAMP, *P_PXL_TIMESTAMP;
U64	PxLTimeGetCurrentTimeMillis();
U64	PxLTimeGetCurrentTimeHighResolution();
void    PxLSleep(U32 millis);
void    PxLGetTimestamp(U64 millis, P_PXL_TIMESTAMP);


////////////////////////////////////////////////////////////////////////////////
// Threading
//
typedef void*	pxlthread_t;

typedef PXL_RETURN_CODE (*pxlthread_function)(pxlthread_t self, void* context);

PXL_RETURN_CODE PxLThreadCreate(pxlthread_t* pThread, pxlthread_function pFn, void* context, char const * pName);
const char*		PxLThreadGetName(pxlthread_t thread);
PXL_RETURN_CODE PxLThreadDestroy(pxlthread_t* pThread); // <-- blocks until the thread completes


////////////////////////////////////////////////////////////////////////////////
// Mutex
//
typedef void* pxlmutex_t;

PXL_RETURN_CODE	PxLMutexInitialize(pxlmutex_t* pMutex, const char* pName);
const char*		PxLMutexGetName(pxlmutex_t mutex);
PXL_RETURN_CODE	PxLMutexLock(pxlmutex_t mutex);
PXL_RETURN_CODE	PxLMutexUnlock(pxlmutex_t mutex);
PXL_RETURN_CODE	PxLMutexDestroy(pxlmutex_t* pMutex);


////////////////////////////////////////////////////////////////////////////////
// Events
//

typedef void* pxlevent_t;
PXL_RETURN_CODE	PxLEventInitialize	(pxlevent_t* pEvent, int autoReset, int initialState, const char* pName);
const char*		PxLEventGetName		(pxlevent_t	event);
int				PxLEventGetState	(pxlevent_t event);
PXL_RETURN_CODE	PxLEventSet			(pxlevent_t	event);
PXL_RETURN_CODE	PxLEventReset		(pxlevent_t	event);
PXL_RETURN_CODE	PxLEventWait		(pxlevent_t event, U32 timeoutInMillis);
PXL_RETURN_CODE PxLEventDestroy		(pxlevent_t* pEvent);

////////////////////////////////////////////////////////////////////////////////
// Misc
//
#ifndef PATH_SEPARATOR
#ifdef PIXELINK_LINUX
#define PATH_SEPARATOR '/'
#else
#define PATH_SEPARATOR '\\'
#endif
#endif


////////////////////////////////////////////////////////////////////////////////
// Tracing
//
void PxLTracePrintf(const char* pFormat, ...);
#if defined (_DEBUG)
#define PxLTraceDebugPrintf(...) do { PxLTracePrintf(__VA_ARGS__); } while(0)
#else
#define PxLTraceDebugPrintf(...) /**/
#endif

#if (defined(PXLTRACE_ENABLED)) || defined(_DEBUG)
// Printing messages
#define PXLTRACE(MSG)												do { PxLTracePrintf(MSG); } while(0)
#define PXLTRACE_PRINT(MSG)											do { PxLTracePrintf(MSG); } while(0)
#define PXLTRACE_PRINTF1(FMT, A1)									do { PxLTracePrintf(FMT, A1);									} while(0)
#define PXLTRACE_PRINTF2(FMT, A1, A2)								do { PxLTracePrintf(FMT, A1, A2);								} while(0)
#define PXLTRACE_PRINTF3(FMT, A1, A2, A3)							do { PxLTracePrintf(FMT, A1, A2, A3);							} while(0)
#define PXLTRACE_PRINTF4(FMT, A1, A2, A3, A4)						do { PxLTracePrintf(FMT, A1, A2, A3, A4);						} while(0)
#define PXLTRACE_PRINTF5(FMT, A1, A2, A3, A4, A5)					do { PxLTracePrintf(FMT, A1, A2, A3, A4, A5);					} while(0)
#define PXLTRACE_PRINTF6(FMT, A1, A2, A3, A4, A5, A6)				do { PxLTracePrintf(FMT, A1, A2, A3, A4, A5, A6);				} while(0)
#define PXLTRACE_PRINTF7(FMT, A1, A2, A3, A4, A5, A6, A7)			do { PxLTracePrintf(FMT, A1, A2, A3, A4, A5, A6, A7);			} while(0)
#define PXLTRACE_PRINTF8(FMT, A1, A2, A3, A4, A5, A6, A7, A8)		do { PxLTracePrintf(FMT, A1, A2, A3, A4, A5, A6, A7, A8);		} while(0)
#define PXLTRACE_PRINTF9(FMT, A1, A2, A3, A4, A5, A6, A7, A8, A9)	do { PxLTracePrintf(FMT, A1, A2, A3, A4, A5, A6, A7, A8, A9);	} while(0)

#else

// Tracing disabled 
#define PXLTRACE(MSG)		/**/
#define PXLTRACE_PRINT(MSG)	/**/
#define PXLTRACE_PRINTF1(FMT, A1)	/**/
#define PXLTRACE_PRINTF2(FMT, A1, A2)	/**/
#define PXLTRACE_PRINTF3(FMT, A1, A2, A3)	/**/
#define PXLTRACE_PRINTF4(FMT, A1, A2, A3, A4)	/**/
#define PXLTRACE_PRINTF5(FMT, A1, A2, A3, A4, A5)	/**/
#define PXLTRACE_PRINTF6(FMT, A1, A2, A3, A4, A5, A6)	/**/
#define PXLTRACE_PRINTF7(FMT, A1, A2, A3, A4, A5, A6, A7)	/**/
#define PXLTRACE_PRINTF8(FMT, A1, A2, A3, A4, A5, A6, A7, A8)	/**/
#define PXLTRACE_PRINTF9(FMT, A1, A2, A3, A4, A5, A6, A7, A8, A9)	/**/

#endif

//
// These can be used to print messages regardless of whether PXLTRACE_ENABLED is defined or not
//

#define PXLTRACE_ALWAYS(MSG)												do { PxLTracePrintf(MSG);										} while(0)
#define PXLTRACE_ALWAYS_PRINT(MSG)											do { PxLTracePrintf(MSG);										} while(0)
#define PXLTRACE_ALWAYS_PRINTF1(FMT, A1)									do { PxLTracePrintf(FMT, A1);									} while(0)
#define PXLTRACE_ALWAYS_PRINTF2(FMT, A1, A2)								do { PxLTracePrintf(FMT, A1, A2);								} while(0)
#define PXLTRACE_ALWAYS_PRINTF3(FMT, A1, A2, A3)							do { PxLTracePrintf(FMT, A1, A2, A3);							} while(0)
#define PXLTRACE_ALWAYS_PRINTF4(FMT, A1, A2, A3, A4)						do { PxLTracePrintf(FMT, A1, A2, A3, A4);						} while(0)
#define PXLTRACE_ALWAYS_PRINTF5(FMT, A1, A2, A3, A4, A5)					do { PxLTracePrintf(FMT, A1, A2, A3, A4, A5);					} while(0)
#define PXLTRACE_ALWAYS_PRINTF6(FMT, A1, A2, A3, A4, A5, A6)				do { PxLTracePrintf(FMT, A1, A2, A3, A4, A5, A6);				} while(0)
#define PXLTRACE_ALWAYS_PRINTF7(FMT, A1, A2, A3, A4, A5, A6, A7)			do { PxLTracePrintf(FMT, A1, A2, A3, A4, A5, A6, A7);			} while(0)
#define PXLTRACE_ALWAYS_PRINTF8(FMT, A1, A2, A3, A4, A5, A6, A7, A8)		do { PxLTracePrintf(FMT, A1, A2, A3, A4, A5, A6, A7, A8);		} while(0)
#define PXLTRACE_ALWAYS_PRINTF9(FMT, A1, A2, A3, A4, A5, A6, A7, A8, A9)	do { PxLTracePrintf(FMT, A1, A2, A3, A4, A5, A6, A7, A8, A9);	} while(0)



// For dumping the contents of memory
void pixelinkTraceDumpMemoryU8 (const U8*	pMem, U32 numElements);
void pixelinkTraceDumpMemoryU16(const U16*	pMem, U32 numElements);
void pixelinkTraceDumpMemoryU32(const U32*	pMem, U32 numElements);


#if (defined(PXLTRACE_ENABLED)) || defined(_DEBUG)
#define PXLTRACE_DUMPMEMU8( ADDR, NUMELEMENTS) pixelinkTraceDumpMemoryU8 (ADDR, NUMELEMENTS);
#define PXLTRACE_DUMPMEMU16(ADDR, NUMELEMENTS) pixelinkTraceDumpMemoryU16(ADDR, NUMELEMENTS);
#define PXLTRACE_DUMPMEMU32(ADDR, NUMELEMENTS) pixelinkTraceDumpMemoryU32(ADDR, NUMELEMENTS);
#else
#define PXLTRACE_DUMPMEMU8( ADDR, NUMELEMENTS) /**/
#define PXLTRACE_DUMPMEMU16(ADDR, NUMELEMENTS) /**/
#define PXLTRACE_DUMPMEMU32(ADDR, NUMELEMENTS) /**/
#endif

#define PXLTRACE_ALWAYS_DUMPMEMU8( ADDR, NUMELEMENTS) pixelinkTraceDumpMemoryU8 (ADDR, NUMELEMENTS);
#define PXLTRACE_ALWAYS_DUMPMEMU16(ADDR, NUMELEMENTS) pixelinkTraceDumpMemoryU16(ADDR, NUMELEMENTS);
#define PXLTRACE_ALWAYS_DUMPMEMU32(ADDR, NUMELEMENTS) pixelinkTraceDumpMemoryU32(ADDR, NUMELEMENTS);


#ifdef __cplusplus

namespace pixelink {
//
// TODO - add profiling option here, to measure and report total time in scope
//
class TraceBlockScope
{
public: 
	TraceBlockScope(const char* blockDesc, bool alwaysPrint=false) :
          m_blockDesc(blockDesc) , m_alwaysPrint(alwaysPrint)
	{
		if (m_alwaysPrint) {
			PXLTRACE_ALWAYS_PRINTF2("%c%s\n", GetInChar(), m_blockDesc);
		} else {
			PXLTRACE_PRINTF2("%c%s\n", GetInChar(), m_blockDesc);
		}
	}

	virtual ~TraceBlockScope()
	{
		if (m_alwaysPrint) {
			PXLTRACE_ALWAYS_PRINTF2("%c%s\n", GetOutChar(), m_blockDesc);
		} else {
			PXLTRACE_PRINTF2("%c%s\n", GetOutChar(), m_blockDesc);
		}
	}

        virtual char GetInChar()  const { return '>'; }
        virtual char GetOutChar() const { return '<'; }
private:
	const char*			m_blockDesc;
	bool				m_alwaysPrint;
};

} // namespace pixelink

#if (defined(PXLTRACE_ENABLED)) || defined(_DEBUG)
#define PXLTRACE_BLOCK(desc)	pixelink::TraceBlockScope pxlTempObj(desc)
#define PXLTRACE_METHOD()		PXLTRACE_BLOCK(__FUNCTION__)
#define PXLTRACE_FUNCTION()		PXLTRACE_BLOCK(__FUNCTION__)
#define PXLTRACE_CTOR()			PXLTRACE_FUNCTION()
#define PXLTRACE_DTOR()			PXLTRACE_FUNCTION()
#else
#define PXLTRACE_BLOCK(desc)	/**/
#define PXLTRACE_METHOD()		/**/
#define PXLTRACE_FUNCTION()		/**/
#define PXLTRACE_CTOR()			/**/
#define PXLTRACE_DTOR()			PXLTRACE_FUNCTION()
#endif

#define PXLTRACE_ALWAYS_BLOCK(desc)		pixelink::TraceBlockScope pxlTempObj(desc, true)
#define PXLTRACE_ALWAYS_METHOD()		PXLTRACE_ALWAYS_BLOCK(__FUNCTION__)
#define PXLTRACE_ALWAYS_FUNCTION()		PXLTRACE_ALWAYS_BLOCK(__FUNCTION__)
#define PXLTRACE_ALWAYS_CTOR()			PXLTRACE_ALWAYS_BLOCK(__FUNCTION__)
#define PXLTRACE_ALWAYS_DTOR()			PXLTRACE_ALWAYS_FUNCTION()


#else  

// Normal C stuff
#if (defined(PXLTRACE_ENABLED)) || defined(_DEBUG)
#define PXLTRACE_BLOCK(desc)	PXLTRACE_PRINTF1("%s\n", desc)
#define PXLTRACE_METHOD()		PXLTRACE_BLOCK(__FUNCTION__)
#define PXLTRACE_FUNCTION()		PXLTRACE_BLOCK(__FUNCTION__)
#else
#define PXLTRACE_BLOCK(desc)	/**/
#define PXLTRACE_METHOD()		/**/
#define PXLTRACE_FUNCTION()		/**/
#endif

#define PXLTRACE_ALWAYS_BLOCK(desc)		PXLTRACE_ALWAYS_PRINTF1("%s\n", desc)
#define PXLTRACE_ALWAYS_METHOD()		PXLTRACE_ALWAYS_BLOCK(__FUNCTION__)
#define PXLTRACE_ALWAYS_FUNCTION()		PXLTRACE_ALWAYS_BLOCK(__FUNCTION__)

#endif // C only

#define PXLTRACE_METHOD0(FMT)				    PXLTRACE_PRINTF1("%s:" FMT, __FUNCTION__)
#define PXLTRACE_METHOD1(FMT, P1)				PXLTRACE_PRINTF2("%s:" FMT, __FUNCTION__, P1)
#define PXLTRACE_METHOD2(FMT, P1, P2)			PXLTRACE_PRINTF3("%s:" FMT, __FUNCTION__, P1, P2)
#define PXLTRACE_METHOD3(FMT, P1, P2, P3)		PXLTRACE_PRINTF4("%s:" FMT, __FUNCTION__, P1, P2, P3)
#define PXLTRACE_METHOD4(FMT, P1, P2, P3, P4)	PXLTRACE_PRINTF5("%s:" FMT, __FUNCTION__, P1, P2, P3, P4)

//
// Generic Helpers
//
#define PXLTRACE_BOOL_AS_STRING(val)	(val) ? "true" : "false"


#ifdef __cplusplus
}
#endif

#endif
