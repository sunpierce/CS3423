#include <8051.h>
#include "cooperative.h"

// declare global variables for shared buffer
__data __at (0x39) char Buffer; // 1-byte data buffer
__data __at (0x3A) char Token; // used in producer
__data __at (0x3B) char Full; // flag indicates whether the buffer is full

// Producer generates 'A' to 'Z' and starts from 'A' aagain
void Producer(void)
{
    // initialize the DS
    Token = 'A';
    // producer thread-yields if buffer is full
    while (1) {
        while (Full) ThreadYield();
        Buffer = Token;
        Full = 1;
        if (Token == 'Z') Token = 'A';
        else Token += 1;
    }
}

// Consumer consumes from Buffer and writes it to the serial port
void Consumer(void)
{
    // initialize the Tx for polling
    TMOD = 0x20;
    TH1 = -6;
    SCON = 0x50;
    TR1 = 1;
    //  consumer thread-yields if buffer is empty or if Tx busy
    while (1) {
        while (!Full) ThreadYield();
        SBUF = Buffer;
        Full = 0;
        while (!TI) ThreadYield();
        TI = 0;
    }
}

// thread-0: one thread acts as producer and another as consumer
void main(void)
{
    // initializes globals
    Full = 0;
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