
#include <stdio.h>

// Serial data
__sfr __at 0x01 ioSerialCmd;
__sfr __at 0x02 ioSerialData;

// Console
__sfr __at 0x03 ioConsolePutChar;
__sfr __at 0x04 ioConsoleRow;
__sfr __at 0x05 ioConsoleCol;
__sfr __at 0x06 ioConsoleColour;

// VRAM memory mode
__sfr __at          0xC8    ioVRAMBankEnable;
__sfr __banked __at 0x07FFF ioVRAMBankDisable;

// VRAM facilities - should be disp
__sfr __at 0x07 ioVRAMPaletteOffset;
__sfr __at 0x08 ioVRAMDraw;
__sfr __at 0x09 ioVRAMSetMode;
__sfr __at 0x14 ioDispClear;

// File System
__sfr __at 0x10 ioFilesysReadWrite;
__sfr __at 0x11 ioFilesysOpenClose;
__sfr __at 0x12 ioFilesysNext;

__sfr __at 0x64 ioSysRAMSize;
__sfr __at 0x65 ioSysVRAMSize;

__sfr __at 0x70 ioSysTimeSliceSet;

unsigned short __at 0x0100 intVectorTable[128];

#define COLOUR_BLACK   0x0000
#define COLOUR_BLUE    0x001F
#define COLOUR_RED     0xF800
#define COLOUR_GREEN   0x07E0
#define COLOUR_CYAN    0x07FF
#define COLOUR_MAGENTA 0xF81F
#define COLOUR_YELLOW  0xFFE0
#define COLOUR_WHITE   0xFFFF

#define SERIAL_CMD_SET_INTVECTOR 0xF1
#define SERIAL_CMD_SET_RATE      0xF2
#define SERIAL_CMD_INIT          0xFA

#define ASSERT(X) if (!(X)) { __asm di __endasm; con_putStringColoured("ASSERT FAIL: ", COLOUR_RED); con_putString(#X); __asm halt __endasm;}


// ZThreading

#define MAX_THREADS 4

#define ZTHREAD_HDL_FREE    0
#define ZTHREAD_NOT_STARTED 1
#define ZTHREAD_RUNNING     2
#define ZTHREAD_STOPPED     3
#define ZTHREAD_TERMINATED  4
#define ZTHREAD_EXITED      5
#define ZTHREAD_WAIT_JOIN   6


typedef char zthread_t;
typedef int (*startFunc_t) (void*);

typedef struct internal_context_s {
  // we actually save the ctx to the sp
  // so we only need the sp to restore all
  unsigned short sp;
} internal_context_t;

typedef struct internal_thread_s {
  startFunc_t startFunc;
  void* arg;
  char flags;
  char state_data;
  unsigned short stack_start;
  internal_context_t ctx;
} internal_thread_t;



#define ZTHREAD_MAX_THREADS_EXCEEDED 0x01
#define ZTHREAD_INVALID_PARAM        0x02
#define ZTHREAD_THREAD_NOT_RUNNING   0x03

#define DBG_THREADLOG(X)
// thread global state
internal_thread_t threads[MAX_THREADS];
char num_threads;
char current_thread;
char thread_schedule_counter;
short stackLocationScratch;

int main( int argc, char* argv[] );
void con_putString(char* s);

void  con_putChar(char);
void  con_setRow (unsigned char);
void  con_setCol (unsigned char);
void  con_newline(void);
void  con_setColour(unsigned short c);
void  con_putStringColoured(char* s, unsigned short colour);

void ihdr_timer_second   ( void ) __naked;
void ihdr_timer_timeSlice( void ) __naked;
void ihdr_ignore         ( void ) __naked;

int _TZL_main_start(void* args) ;

void _TZL_start_threads( void );

#define KERNEL_STACK_LOC 0x07F0

// Always the first function in the compilation unit
void _TZL_start_( void ) __naked {
  __asm
    ; load the stack pointer to the kernel stack
    ld sp, #0x07F0
    
  __endasm;
  
  _TZL_start_threads();
}


void SetStack(unsigned short value) __naked {
  value;
  __asm
    pop hl 
    ld sp, hl
    push hl
    ret
  __endasm;
}



void _TZL_start_threads( void ) {
  unsigned char i = 0;
  short arg_store[2];  

  con_setRow(0);
  con_setCol(0);
  
  DBG_THREADLOG( con_putString("Setting up interrupt table...");)
  
  for (; i < 16; i++) {
    intVectorTable[i] = (unsigned short)(char*)&ihdr_ignore;
  }
  
  DBG_THREADLOG( con_putString("OK");)
  DBG_THREADLOG( con_newline();)
  
  num_threads = 1;
  current_thread = 0;
  
  DBG_THREADLOG( con_putString("Setting up thread 0...");)
  
  threads[0].startFunc = _TZL_main_start;
  threads[0].arg = &arg_store;
  threads[0].flags = ZTHREAD_RUNNING;
  threads[0].state_data = 0;
  threads[0].stack_start = 0x0400;
  
  threads[1].flags = ZTHREAD_HDL_FREE;
  threads[1].state_data = 0;
  threads[1].stack_start = 0x0500;
  
  threads[2].flags = ZTHREAD_HDL_FREE;
  threads[2].state_data = 0;
  threads[2].stack_start = 0x0600;
  
  threads[3].flags = ZTHREAD_HDL_FREE;
  threads[3].state_data = 0;
  threads[3].stack_start = 0x0700;
  
  DBG_THREADLOG( con_putString("OK");)
  DBG_THREADLOG( con_newline();)

  
  DBG_THREADLOG( con_putString("Registering timeslice interrupt...");)
  
  intVectorTable[3] = (unsigned short)(char*)&ihdr_timer_timeSlice;;
  
  DBG_THREADLOG( con_putString("OK");)
  DBG_THREADLOG( con_newline();)
  
  {
   __asm
    im 2
    ld a, #0x01
    ld i, a
  __endasm;
  }
  
    
  DBG_THREADLOG( con_putString("Switching stack...");)
  // switch to thread 0
  //SetStack((unsigned short) threads[0].stack_start-2);
  __asm
    ; load the stack pointer to the thread0 stack
    ld sp, #0x0400
  __endasm;
  
  DBG_THREADLOG( con_putString("OK");)
  DBG_THREADLOG( con_newline();)
  
  DBG_THREADLOG( con_putString("Entering Thread 0.");)
  
  (*threads[0].startFunc)(threads[0].arg);
  
}


int _TZL_main_start(void* args) {
  void** args_array = (void**)args;
  return main((int)args_array[0], (char*[])args_array[1]);
}

int startFunc_print(void* args) {
  char c = (char)args;
  while (1) {
    con_putChar(c);
  }
}

int zthread_create(zthread_t* handle, startFunc_t startFunc, void* args) {
  char this_id = -1;
  char idx = 0;
  
  if ((startFunc == NULL) || (handle == NULL)) {
    return ZTHREAD_INVALID_PARAM;
  }
  
  num_threads++;
  
  // find a free handle
  for (; idx < MAX_THREADS; idx++) {
    if (threads[idx].flags == ZTHREAD_HDL_FREE) {
      this_id = idx;
      break;
    }
  }
  
  if (this_id <0) {
    return ZTHREAD_MAX_THREADS_EXCEEDED;
  }
  
  threads[this_id].startFunc = startFunc;
  threads[this_id].arg = args;
  threads[this_id].flags = ZTHREAD_NOT_STARTED;
  threads[this_id].state_data = 0;
  
  *handle = this_id;
  
  return 0;
}

zthread_t zthread_getThread( void ) {
  return (zthread_t)current_thread;
}

// suspend this thread until the provided thread
// exits.
int zthread_join(zthread_t handle) {
  zthread_t thisThread = zthread_getThread();
  if (threads[thisThread].flags != ZTHREAD_RUNNING) {
    return ZTHREAD_THREAD_NOT_RUNNING;
  }
  
  // if the thread we want to join with is marked
  // as free, assume it's already exited and so 
  // return. This should be the exited flag, really
  if (threads[handle].flags == ZTHREAD_HDL_FREE) {
    return 0;
  }
  
  if (threads[handle].flags != ZTHREAD_RUNNING) {
    if (threads[handle].flags != ZTHREAD_WAIT_JOIN) {
      return ZTHREAD_THREAD_NOT_RUNNING;
    }
  }
  
  threads[thisThread].flags = ZTHREAD_WAIT_JOIN;
  threads[thisThread].state_data = handle;
  
  __asm
    halt
  __endasm;
  
  return 0;
}

void _TZL_thread_exited( void ) {
  char idx = 0;
  zthread_t thisThread = zthread_getThread();
  
  // this needs to be in a critical section.. so disable interrupts
  __asm 
    di
  __endasm;

  // if any threads are joining to us, tell them they can 
  // continue now
  for (; idx < MAX_THREADS; idx++) {
    if ((threads[idx].flags == ZTHREAD_WAIT_JOIN)
      && (threads[idx].state_data == thisThread)) {
      threads[idx].flags = ZTHREAD_RUNNING;
      threads[idx].state_data = 0;
    }
  }
  
  // For now, just set the flag as free. 
  // Really we should set as exited and we can then
  // look to get the return value.
  threads[thisThread].flags = ZTHREAD_HDL_FREE;
  
  // this thread ends here. halt so we can be swapped out.
  __asm
    ei
    halt
  __endasm;
}

int zthread_start(zthread_t handle) {
  short* stack;
  // as writes on the z80 are technically atomic, 
  // as long as the last write in this function is the
  // flags set we do not need to be in a critical section!
  threads[handle].ctx.sp = ((unsigned short)threads[handle].stack_start)-16;
  stack = (short*)threads[handle].ctx.sp;
  stack[0] = 0; //  pop iy
  stack[1] = 0; //  pop ix
  stack[2] = 0; //  pop de
  stack[3] = 0; //  pop af
  stack[4] = 0; //  pop bc
  stack[5] = 0; //  pop hl
  stack[6] = (short)threads[handle].startFunc;
  stack[7] = (short)_TZL_thread_exited;
  stack[8] = (short)threads[handle].arg;
  threads[handle].flags = ZTHREAD_RUNNING;
  return 0;
}

void ihdr_ignore( void ) __naked {
  __asm
    reti
  __endasm;
}

void ihdr_timer_second( void ) __naked {
  __asm
    di
  __endasm;
  con_putString(".");
  __asm
    ei
    reti
  __endasm;
}

void panic_deadlock( void ) {
  char buffer[32];
  char idx = 0;
  ioDispClear = 0;
  con_setCol(8);
  con_setRow(2);
  con_putStringColoured("*** DEADLOCK DETECTED ***", COLOUR_RED);
  con_setCol(8);
  con_setRow(3);
  con_putStringColoured("=========================", COLOUR_YELLOW);
  con_setCol(0);
  con_setRow(5);
  con_putStringColoured("NO THREADS IN A STATE TO RUN", COLOUR_WHITE);
  con_newline();
  con_newline();
  con_putString("  IDX  FLAGS  STATE");
  con_newline();
  con_putString(" ====================");
  con_newline();
  for (idx=0; idx<MAX_THREADS; idx++) {
    sprintf(&buffer[0], "  %d:   0x%X    0x%X ", 
      idx, 
      threads[idx].flags,
      threads[idx].state_data);
    con_newline();
    con_putString(buffer);
  }
  // as interrupts are disabled, only a nmi can restore this halt
  __asm
    halt
  __endasm;
}
    

void ihdr_timer_timeSlice( void ) __naked {
  // save state (PC is already on stack from interrupt ack
  __asm
    di
    push hl
    push bc
    push af
    push de
    push ix
    push iy
    ld (_stackLocationScratch), sp
    exx
    ex af, af'
  __endasm;
  
  // save stack to current thread ctx
  threads[current_thread].ctx.sp = stackLocationScratch;
  
  // Choose next thread to run 
  thread_schedule_counter = 0;
  do {
    thread_schedule_counter++;
    current_thread++;
    if (current_thread >= MAX_THREADS) {
      current_thread = 0;
    }
  } while ((threads[current_thread].flags != ZTHREAD_RUNNING)
    && (thread_schedule_counter <= MAX_THREADS));
  
  if (thread_schedule_counter > MAX_THREADS) {
    // swap to the kernel stack for this
    __asm
      ; load the stack pointer to the kernel stack
      ld sp, #0x07F0
    __endasm;
  
    panic_deadlock();
  }
  
  // load stack of next thread ctx
  stackLocationScratch = threads[current_thread].ctx.sp;
  
  // restore registers
  __asm
    ex af, af'
    exx
    ld sp, (_stackLocationScratch)
    pop iy
    pop ix
    pop de
    pop af
    pop bc
    pop hl
    ei
    reti
  __endasm;

}

void serial_setIntVector( unsigned char vector ) {
  ioSerialCmd = SERIAL_CMD_SET_INTVECTOR;
  ioSerialCmd = vector;
}

void serial_setRate(short rate) {
  ioSerialCmd = SERIAL_CMD_SET_RATE;
  ioSerialCmd = (unsigned char)(rate & 0xFF);
  ioSerialCmd = (unsigned char)(rate >> 8U);
}

void serial_init( void ) {
  ioSerialCmd = SERIAL_CMD_INIT;
}

char serial_avail( void ) {
  return ioSerialCmd;
}

char serial_getChar( void ) {
  while (!ioSerialCmd);
  return ioSerialData;
}

void serial_putChar( char c ) {
  ioSerialData = c;
}

void disp_enableVRAMWrite() {
  __asm
    di
  __endasm;
  ioVRAMBankEnable = 0;
}

void disp_disableVRAMWrite() {
  char c = ioVRAMBankDisable;
  __asm
    ei
  __endasm;
}

enum displayMode_e {
  mode48x48x16 = 0,
  mode48x48x8P,
  mode48x48x8PStretch,
  mode160x120x8P,
  mode160x120x8PStretch,
  mode40x30x16Stretch,
  mode80x60x8PStretch,
  mode80x60x8P,
  };

void disp_setMode(enum displayMode_e mode) {
  ioVRAMSetMode = (unsigned char)mode;
}

void disp_draw( void ) {
  ioVRAMDraw = 0;
}

void disp_clear( void ) {
  ioDispClear = 0;
}

short sys_getRAMSize(void) {
  return (ioSysRAMSize<<8U);
}

short sys_getVRAMSize(void) {
  return (ioSysVRAMSize<<8U);
}

void con_putChar(char c) {
  ioConsolePutChar = c;
}

void con_putString(char* s) {
  while (*s) {
    con_putChar(*s);
    s++;
  }
}

void con_setRow(unsigned char r) {
  ioConsoleRow = r;
}

unsigned char con_getRow(void) {
  return ioConsoleRow;
}

void con_setCol(unsigned char c) {
  ioConsoleCol = c;
}

unsigned char con_getCol(void) {
  return ioConsoleCol;
}

void con_newline(void) {
  unsigned char row = con_getRow();
  row++;
  con_setRow(row);
  con_setCol(0);
}

void con_newlines(unsigned char num) {
  for (; num>0; num--) {
    con_newline();
  }
}

void con_setColour(unsigned short c) {
  ioConsoleColour = (unsigned char)(c & 0xFF);
  ioConsoleColour = (unsigned char)(c >> 8U);
}

void con_putStringColoured(char* s, unsigned short colour) {
  con_setColour(colour);
  con_putString(s);
}

int startFunc_print2(void* args) {
  char c = (char)args;
  short num = 400;
  while (num--) {
    con_putChar(c);
  }
  return 0;
}

int startFunc_print_deadlock(void* args) {
  char c = (char)args;
  char num = 140;
  while (num--) {
    con_putChar(c);
  }
  
  // main thread always id 0
  ASSERT(! zthread_join(0));
  return 0;
}


int main( int argc, char* argv[] ) {

  zthread_t threadA;
  zthread_t threadB;
  zthread_t threadC;
  
  argc;
  argv;
  
  ASSERT(! zthread_create(&threadA, startFunc_print2, (char*)(short)'A'));
  ASSERT(! zthread_create(&threadB, startFunc_print2, (char*)(short)'B'));
  ASSERT(! zthread_create(&threadC, startFunc_print2, (char*)(short)'C'));
  
  ASSERT(! zthread_start(threadA));
  ASSERT(! zthread_start(threadB));
  ASSERT(! zthread_start(threadC));
  
  __asm
    ei
  __endasm;

  
  ASSERT(! zthread_join(threadA));
  
  ASSERT(! zthread_join(threadB));
  
  ASSERT(! zthread_join(threadC));
  
  con_putString(" Thread A,B & C has exited, main thread can continue to deadlock detection test! ");
  
  ASSERT(! zthread_create(&threadA, startFunc_print_deadlock, (char*)(short)'!'));
  ASSERT(! zthread_start(threadA));
  
  startFunc_print2((char*)(short)'P');
  
  ASSERT(! zthread_join(threadA));
  
  while(1) {
    con_putChar('M');
  }

  return 0;
}





