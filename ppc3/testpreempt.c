#include <8051.h>
#include "preemptive.h"

// declare global variables for shared buffer
__data __at (0x3A) char Buffer[3]; // 3-byte data buffer
__data __at (0x3D) char Full; // flag indicates whether the buffer is full & semaphore
__data __at (0x3E) char mutex; // semaphore
__data __at (0x3F) char Empty; // semaphore
__data __at (0x20) char Token; // used in producer
__data __at (0x21) char head; // for 3-byte data buffer
__data __at (0x22) char tail; // for 3-byte data buffer

// we need to give each maro-expanded instance of inlined assembly code a different label so the insc can jump to the proper label
// could be solved by using C preprocessor's __COUNTER__ name, which generates a new integer literal each time it is used.
// just ensure that you concatenate __COUNTER__ wiht the $ to form the label
// __COUNTER__ is macro so macro expansion is necessary
#define L(x) Label(x)
#define Label(x) x##$

// Producer generates 'A' to 'Z' and starts from 'A' aagain
void Producer(void)
{
    Token = 'A';
    while (1) {
        SemaphoreWait(Empty, L(__COUNTER__));
        SemaphoreWait(mutex, L(__COUNTER__));
        EA = 0; // access shared variable => should be executed atomically
        Buffer[head] = Token;
        head = (head < 2)? head + 1: 0;
        if (Token == 'Z') Token = 'A';
        else Token += 1;
        EA = 1;
        SemaphoreSignal(mutex);
        SemaphoreSignal(Full);
    }
}

// Consumer consumes from Buffer and writes it to the serial port
void Consumer(void)
{
    // initialize the Tx for polling
    EA = 0; // TMOD is shared variable
    TMOD |= 0x20; // TMOD is also modified be Bootstrap code to set up the timer interrupt in timer-0 for preemption
    TH1 = -6;
    SCON = 0x50;
    TR1 = 1;
    EA = 1; // end of critical section
    //  consumer thread-yields if buffer is empty or if Tx busy
    while (1) {
        SemaphoreWait(Full, L(__COUNTER__));
        SemaphoreWait(mutex, L(__COUNTER__));
        EA = 0; // access shared variable
        SBUF = Buffer[tail];
        tail = (tail < 2)? tail + 1: 0;
        EA = 1;
        SemaphoreSignal(mutex);
        SemaphoreSignal(Empty);
        while (!TI);
        TI = 0;
    }
}

// thread-0: one thread acts as producer and another as consumer
void main(void)
{
    // initializes globals (semaphores)
    SemaphoreCreate(mutex, 1);
    SemaphoreCreate(Empty, 3);
    SemaphoreCreate(Full, 0);
    head = 0;
    tail = 0;
    // set up producer and consumer
    ThreadCreate(Producer);
    Consumer();
}

// start-up
void _sdcc_gsinit_startup(void) 
{
        __asm
            ljmp  _Bootstrap
        __endasm;
}

void _mcs51_genRAMCLEAR(void) {}
void _mcs51_genXINIT(void) {}
void _mcs51_genXRAMCLEAR(void) {}
void timer0_ISR(void) __interrupt(1) {
        __asm
                ljmp _myTimer0Handler
        __endasm;
}
