// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <interrupts.h>
#include <keyboardDriver.h>
#include <lib.h>
#include <memoryManager.h>
#include <stddef.h>
#include <stringLib.h>
#include <taskManager.h>
#include <videoDriver.h>
#include <timerTick.h>

#define SIZE_OF_STACK (4 * 1024)
#define DEFAULT_FG_PRIORITY 2
#define DEFAULT_BG_PRIORITY 1
#define MAX_PRIORITY 60

typedef struct {
      uint64_t gs;
      uint64_t fs;
      uint64_t r15;
      uint64_t r14;
      uint64_t r13;
      uint64_t r12;
      uint64_t r11;
      uint64_t r10;
      uint64_t r9;
      uint64_t r8;
      uint64_t rsi;
      uint64_t rdi;
      uint64_t rbp;
      uint64_t rdx;
      uint64_t rcx;
      uint64_t rbx;
      uint64_t rax;

      uint64_t rip;
      uint64_t cs;
      uint64_t eflags;
      uint64_t rsp;
      uint64_t ss;
      uint64_t base;
} t_stackFrame;

typedef enum { READY,
               BLOCKED,
               KILLED } t_state;

typedef struct {
      uint64_t pid;
      uint64_t ppid;
      uint64_t priority;
      uint8_t fg;
      uint16_t infd;
      uint16_t outfd;
      char name[20];
      void* rsp;
      void* rbp;
      t_state state;
} t_PCB;

typedef struct t_pNode {
      t_PCB pcb;
      struct t_pNode* next;
} t_pNode;

typedef struct pList {
      uint32_t size;
      uint32_t readyProcesses;
      t_pNode* first;
      t_pNode* last;
} t_pList;

static void initializeStackFrame(void (*entryPoint)(int, char**), int argc, char** argv, void* rbp);
static int initProcess(t_PCB* process, char* name, uint8_t fg, uint16_t * fd);
static uint64_t newPid();
static void idleFunction(int argc, char** argv);
static void wrapper(void (*entryPoint)(int, char**), int argc, char** argv);
static void exit();
static void copyArgs(char** dest, char** source, int count);

static void enqueueProcess(t_pNode* newProcess);
static t_pNode* dequeueProcess();
static int queueIsEmpty();
static void removeProcess(t_pNode* process);
static void dumpProcesses();
static void dumpProcess(t_PCB process);
static t_pNode* getProcessByPID(uint64_t pid);
static int changeState(uint64_t pid, t_state state);

static t_pList* processes;
static t_pNode* currentProcess;
static uint64_t currentProcessTicksLeft;
static t_pNode* idleProcess;
static uint64_t maxPid = 1;

void initScheduler() {
      processes = mallocBR(sizeof(t_pList));
      processes->first = NULL;
      processes->last = processes->first;
      processes->size = 0;
      processes->readyProcesses = 0;

      char* argv[] = {"System Idle Process"};
      addProcess(&idleFunction, 1, argv, 1, 0);
      idleProcess = dequeueProcess();
}

void* scheduler(void* oldRSP) {
      if (currentProcess != NULL) {
            if (currentProcess->pcb.state == READY && currentProcessTicksLeft > 0) {
                  currentProcessTicksLeft--;
                  return oldRSP;
            }

            currentProcess->pcb.rsp = oldRSP;

            if (currentProcess->pcb.pid != idleProcess->pcb.pid) {
                  if (currentProcess->pcb.state == KILLED) {
                        t_pNode* parent = getProcessByPID(currentProcess->pcb.ppid);
                        if (parent != NULL && currentProcess->pcb.fg && parent->pcb.state == BLOCKED){
                              unblockProcess(parent->pcb.pid);
                        }
                        removeProcess(currentProcess);
                  } else{
                        enqueueProcess(currentProcess);
                  }
            }
      }
      if (processes->readyProcesses > 0) {
            currentProcess = dequeueProcess();
           
            while (currentProcess->pcb.state != READY) {
                  if (currentProcess->pcb.state == KILLED) {
                        removeProcess(currentProcess);
                  } else if (currentProcess->pcb.state == BLOCKED)
                        enqueueProcess(currentProcess);

                  currentProcess = dequeueProcess();
            }

      } else{
            currentProcess = idleProcess;
      }

      currentProcessTicksLeft = currentProcess->pcb.priority;
      return currentProcess->pcb.rsp;
}

uint64_t addProcess(void (*entryPoint)(int, char**), int argc, char** argv, uint8_t fg, uint16_t * fd) {
      if (entryPoint == NULL)
            return 0;

      t_pNode* newProcess = mallocBR(sizeof(t_pNode));

      if (newProcess == 0)
            return 0;

      if (initProcess(&newProcess->pcb, argv[0], fg, fd) == 1)
            return 0;

      char** argvCpy = mallocBR(sizeof(char*)*argc);
      if (argvCpy == 0)
            return 0;
      copyArgs(argvCpy, argv, argc);

      initializeStackFrame(entryPoint, argc, argvCpy, newProcess->pcb.rbp);
      enqueueProcess(newProcess);

      if (newProcess->pcb.fg && newProcess->pcb.ppid)
            blockProcess(newProcess->pcb.ppid);


      return newProcess->pcb.pid;
}

void listProcesses() {
      printfBR("PID    PPID    CMD    FG    PRIO    STATE    RSP    RBP\n");
      if (currentProcess != NULL)
            dumpProcess(currentProcess->pcb);

      dumpProcesses();
}

uint64_t killProcess(uint64_t pid) {
      int aux = changeState(pid, KILLED);

      if (pid == currentProcess->pcb.pid) 
            callTimerTick();

      return aux;
}

void resignCPU() {
      exit();
}

void wait(uint64_t pid){
      t_pNode * p=getProcessByPID(pid);
      if (p != NULL) {
            p->pcb.fg=1;
            blockProcess(currentProcess->pcb.pid);
      }
}

void yield() {
      currentProcessTicksLeft = 0;
      callTimerTick();
}

uint64_t changePriority(uint64_t pid, uint64_t priority) {
      if (priority > MAX_PRIORITY)
            return 0;

      t_pNode* p = getProcessByPID(pid);
      if (p != NULL) {
            p->pcb.priority = priority;
            return pid;
      }
      return 0;
}

int getCurrentProcessInFd(){
      return currentProcess->pcb.infd;
}

int getCurrentProcessOutFd() {
      return currentProcess->pcb.outfd;
}

int currentProcessFg(){
      return currentProcess->pcb.fg;
}

void killForeground() {
      if (currentProcess != NULL && currentProcess->pcb.fg && currentProcess->pcb.state == READY){
            killProcess(currentProcess->pcb.pid);
            return;
      }
}

uint64_t blockProcess(uint64_t pid) {
      int aux = changeState(pid, BLOCKED);

      if (pid == currentProcess->pcb.pid) {
            callTimerTick();
      }

      return aux;
}

uint64_t unblockProcess(uint64_t pid) {
      return changeState(pid, READY);
}

uint64_t currentProcessPid() {
      return currentProcess ? currentProcess->pcb.pid : 0;
}

static int changeState(uint64_t pid, t_state state) {
      t_pNode* p = getProcessByPID(pid);

      if (p == NULL || p->pcb.state == KILLED) 
            return -1;

      if (p == currentProcess) {
            if (currentProcess->pcb.state == state)
                  return 1;
            currentProcess->pcb.state = state;
            return 0;
      }

      if (p->pcb.state == state)
            return 1;

      if (p->pcb.state != READY && state == READY)
            processes->readyProcesses++;

      else if (p->pcb.state == READY && state != READY)
            processes->readyProcesses--;

      p->pcb.state = state;

      return 0;
}

static int initProcess(t_PCB* process, char* name, uint8_t fg, uint16_t * fd) {
      strcpy(process->name,name);

      process->pid = newPid();
      process->ppid = currentProcess == NULL ? 0 : currentProcess->pcb.pid;

      if (fg > 1)
            return 1;
      process->fg = currentProcess == NULL ? fg : (currentProcess->pcb.fg ? fg : 0);

      process->rbp = mallocBR(SIZE_OF_STACK);
      if (process->rbp == NULL)
            return 1;

      process->rbp = (void*)((char*)process->rbp + SIZE_OF_STACK - 1);
      process->rsp = (void*)((t_stackFrame*)process->rbp - 1);
      process->state = READY;
      process->infd = fd ? fd[0] : 0;
      process->outfd = fd ? fd[1] : 0;
      process->priority = process->fg ? DEFAULT_FG_PRIORITY : DEFAULT_BG_PRIORITY;

      return 0;
}

static void wrapper(void (*entryPoint)(int, char**), int argc, char** argv) {
      entryPoint(argc, argv);
      for (int i = 0; i < argc; i++) {
            freeBR(argv[i]);
      }
      freeBR(argv);
      exit();
}

static void copyArgs(char** dest, char** source, int count) {
      for (int i = 0; i < count; i++) {
            dest[i] = mallocBR(sizeof(char) * (strlen(source[i]) + 1));
            strcpy(dest[i], source[i]);
      }
}

static void initializeStackFrame(void (*entryPoint)(int, char**), int argc, char** argv, void* rbp) {
      t_stackFrame* frame = (t_stackFrame*)rbp - 1;
      frame->gs = 0x001;
      frame->fs = 0x002;
      frame->r15 = 0x003;
      frame->r14 = 0x004;
      frame->r13 = 0x005;
      frame->r12 = 0x006;
      frame->r11 = 0x007;
      frame->r10 = 0x008;
      frame->r9 = 0x009;
      frame->r8 = 0x00A;
      frame->rsi = (uint64_t)argc;
      frame->rdi = (uint64_t)entryPoint;
      frame->rbp = 0x00D;
      frame->rdx = (uint64_t)argv;
      frame->rcx = 0x00F;
      frame->rbx = 0x010;
      frame->rax = 0x011;
      frame->rip = (uint64_t)wrapper;
      frame->cs = 0x008;
      frame->eflags = 0x202;
      frame->rsp = (uint64_t)(&frame->base);
      frame->ss = 0x000;
      frame->base = 0x000;
}

static uint64_t newPid() {
      return maxPid++;
}

static int queueIsEmpty() {
      return processes->size == 0;
}

static void enqueueProcess(t_pNode* newProcess) {
      if (queueIsEmpty()) {
            processes->first = newProcess;
            processes->last = processes->first;
      } else {
            processes->last->next = newProcess;
            newProcess->next = NULL;
            processes->last = newProcess;
      }

      if (newProcess->pcb.state == READY){
            processes->readyProcesses++;
      }

      processes->size++;
}

static void dumpProcess(t_PCB process) {
      char* state;
      switch (process.state) {
            case READY:
                  state = "READY";
                  break;
            case BLOCKED:
                  state = "BLOCKED";
                  break;
            default:
                  state = "KILLED";
                  break;
      }
      printfBR("%d      %d      %s    %d    %d    %s    %x    %x    \n", process.pid, process.ppid, process.name, process.fg, process.priority, state, (uint64_t)process.rsp, (uint64_t)process.rbp);
}

static void dumpProcesses() {
      for (t_pNode* p = processes->first; p != NULL; p = p->next) {
            dumpProcess(p->pcb);
      }
}

static t_pNode* dequeueProcess() {
      if (queueIsEmpty())
            return NULL;

      t_pNode* p = processes->first;
      processes->first = processes->first->next;
      processes->size--;

      if (p->pcb.state == READY)
            processes->readyProcesses--;

      return p;
}

static void removeProcess(t_pNode* process) {
      freeBR((char*)process->pcb.rbp - SIZE_OF_STACK + 1);
      freeBR(process);
}

static void idleFunction(int argc, char** argv) {
      while (1){
            _hlt();
      }
}

static void exit() {
      killProcess(currentProcess->pcb.pid);
      callTimerTick();
}

static t_pNode* getProcessByPID(uint64_t pid) {
      if (currentProcess != NULL) {
            if (currentProcess->pcb.pid == pid)
                  return currentProcess;
      }

      for (t_pNode* p = processes->first; p != NULL; p = p->next) {
            if (p->pcb.pid == pid)
                  return p;
      }

      return NULL;
}
