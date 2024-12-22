#include <8051.h>
#include "preemptive.h"

// declare the static globals (char takes up 1 byte)
// manually allocated memory in the range 0x20-0x3F
__data __at (0x30) char savedSP[4];
__data __at (0x34) char bitmap;
__data __at (0x35) char cur; // current thread ID
__data __at (0x36) char i; // index
__data __at (0x37) char tempSP; // used in ThreadCreate
__data __at (0x38) char newThread; // used in ThreadCreate
__data __at (0x39) char temp;

// macro for saving the context of the current thread
#define SAVESTATE \
    { \
        __asm \
            PUSH ACC; \
            PUSH B; \
            PUSH DPL; \
            PUSH DPH; \
            PUSH PSW; \
        __endasm; \
        savedSP[cur] = SP; \
    }

// macro for restoring the context of the current thread
#define RESTORESTATE \
    { \
        SP = savedSP[cur]; \
        __asm \
            POP PSW; \
            POP DPH; \
            POP DPL; \
            POP B; \
            POP ACC; \
        __endasm; \
    }

// reference main's symbol when creating a thread for it
extern void main(void);

// Bootstrap is jumped to by the startup code to make the thread for main
void Bootstrap(void)
{
    // initialize thread mgr vars
    bitmap = 0;
    // set up timer
    TMOD = 0; // 0 for software control and 13-bit timer (mode 0)
    IE = 0x82; // enable timer-0 interrupt
    TR0 = 1; // start timer 0
    // create thread for main and set current thread ID
    cur = ThreadCreate(main);
    // restore so that it starts running main()
    RESTORESTATE;
}

// ThreadCreate() creates a thead DS so it is ready to be restored
ThreadID ThreadCreate(FunctionPtr fp)
{
    // check whether reach the max #threads
    if (bitmap == 15) return -1; // bitmap == 1111b means four threads are all in used
    // start of critical section (make sure it is executed atomically)
    EA = 0;
        // find one that is not in used
        for (i = 0; i < 4; i++) {
            if (!(bitmap & (1<<i))) {
                // update bitmap
                bitmap |= (1<<i); // change that thread to 1
                // update current thread ID
                newThread = i;
                break;
            }
        }
        // save the current sp in temp and set sp to the starting location for the new thread
        tempSP = SP;
        // thread0: 40H-4FH, thread1: 50H-5FH, thread2: 60H-6FH, thread3: 70H-7FH
        SP = (0x3F) + newThread * (0x10);
        // push return address fp so that it can jump to fp
        __asm 
            PUSH DPL;
            PUSH DPH;
        __endasm;
        // initialization part
        // initialize the four reg's (ACC, B, DPL, DPH) to zero
        __asm 
            MOV A, #0
            PUSH ACC 
            PUSH ACC 
            PUSH ACC 
            PUSH ACC 
        __endasm;
        // initialize PSW (CY, AC, F, RS1, RS0, OV, UD, P) only need to set PSW.4 and PSW.3
        PSW = newThread << 3;
        __asm 
            PUSH PSW
        __endasm;
        // update sp
        savedSP[newThread] = SP;
        // restore sp from tempSP
        SP = tempSP;
    EA = 1; // end of critical section
    // return the newly created thead ID
    return newThread;
}

// ThreadYield: voluntarily yield the control to another thread
void ThreadYield(void)
{
    EA = 0; // make sure it is executed atomically
        // save the context of the current thread
        SAVESTATE;
        do {
            // RR scheduling
            cur = (cur + 1) % 4;
            // check whether that picked thread is ready to run
            if (bitmap & (1<<cur)) break;
            // otherwise pick next one
        } while (1);
        // restore the context of the another thread
        RESTORESTATE;
    EA = 1; // end of critical section
}

// ThreadExit: terminate itself and switch context to antoher thread
void ThreadExit(void)
{
    EA = 0; // make sure it is executed atomically
        // clear bit for the current thread from the bitmap
        bitmap &= ~(1 << cur);
        // pick another available thread
        for (i = 0; i < 4; i++) {
            if (bitmap & (1 << i)) {
                cur = i;
                break;
            }
        }
        // if no ready thread to pick, set current thread ID to -1
        if (i == 4) cur = -1;
        // restore the context of the another thread
        RESTORESTATE;
    EA = 1; // end of critical section
}

void myTimer0Handler()
{
    EA = 0; // start of critical section (disable interrupt)
    SAVESTATE;
    // C code might use R0-R7 so we insert code to preserve the value of any such registers
    __asm 
        MOV A, R0
        PUSH ACC 
        MOV A, R1 
        PUSH ACC 
        MOV A, R2
        PUSH ACC 
        MOV A, R3
        PUSH ACC 
        MOV A, R4 
        PUSH ACC 
        MOV A, R5 
        PUSH ACC 
        MOV A, R6 
        PUSH ACC 
        MOV A, R7 
        PUSH ACC 
    __endasm;
    // pick up the next thread
    /* NOT working
     while (1) {
        cur = (cur + 1) % 4;
        if (bitmap & (1<<cur)) break;
    } 
    */
    while (1) {
        cur = (cur < 3)? cur+1: 0;
        if (cur == 0) temp = 1;
        else if (cur == 1) temp = 2;
        else if (cur == 2) temp = 4;
        else temp = 8;
        if (bitmap & temp) break;
    }
    __asm 
        POP ACC 
        MOV R7, A 
        POP ACC 
        MOV R6, A 
        POP ACC 
        MOV R5, A 
        POP ACC 
        MOV R4, A 
        POP ACC 
        MOV R3, A 
        POP ACC 
        MOV R2, A 
        POP ACC 
        MOV R1, A 
        POP ACC 
        MOV R0, A 
    __endasm;
    RESTORESTATE;
    EA = 1; // end of critical section (re-enable interrupt)
    // return from interrupt
    __asm 
        RETI 
    __endasm;
}