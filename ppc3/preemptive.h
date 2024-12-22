#ifndef __PREEMPTIVE_H__
#define __PREEMPTIVE_H__

#define MAXTHREADS 4

// Define the macro for converting a C name into an assembly name
// CNAME(mutex): it gets macro-expanded into _mutex
#define CNAME(s) _ ## s

// Initialize the semaphore s to the value n as a macro of inlined code
#define SemaphoreCreate(s, n) \
    { \
        s = n; \
    }

#define SemaphoreWait(s, LABEL) \
    { \
        __asm \
LABEL: \
            MOV A, CNAME(s) \
            JZ LABEL \
            JB ACC.7, LABEL \
            dec CNAME(s) \
        __endasm; \
    }

// Increment the semaphore variable
// The operand of the asm instruction can refer to a global var name defined in C by prepending it with an underscore
#define SemaphoreSignal(s) \
    { \
        __asm \
            INC CNAME(s) \
        __endasm; \
    }

typedef char ThreadID;
typedef void (*FunctionPtr)(void);

ThreadID ThreadCreate(FunctionPtr);
void ThreadYield(void);
void ThreadExit(void);
void myTimer0Handler();

#endif
