// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <RTCTime.h>
#include <keyboardDriver.h>
#include <lib.h>
#include <memoryManager.h>
#include <semaphores.h>
#include <pipes.h>
#include <stringLib.h>
#include <sysCallDispatcher.h>
#include <taskManager.h>
#include <IOManager.h>
#include <timerTick.h>
#include <videoDriver.h>

#define SYS_GETMEM_ID 0
#define SYS_RTC_TIME_ID 1
#define SYS_TEMP_ID 2
#define SYS_WRITE_ID 3
#define SYS_GETCHAR_ID 4
#define SYS_CLEAR_ID 5
#define SYS_LOAD_APP_ID 6
#define SYS_INFOREG_ID 7
#define SYS_PS_ID 8
#define SYS_SECS_ELAPSED_ID 9
#define SYS_KILL_ID 10
#define SYS_NICE_ID 11
#define SYS_BLOCK_ID 12
#define SYS_UNBLOCK_ID 13
#define SYS_GETPID_ID 14
#define SYS_MALLOC_ID 15
#define SYS_FREE_ID 16
#define SYS_YIELD_ID 17
#define SYS_SEM_OPEN_ID 18
#define SYS_SEM_CLOSE_ID 19
#define SYS_SEM_POST_ID 20
#define SYS_SEM_WAIT_ID 21
#define SYS_SEM_DUMP_ID 22
#define SYS_PIPE_POPEN_ID 23
#define SYS_PIPE_CLOSE_ID 24
#define SYS_PIPE_WRITE_ID 25
#define SYS_PIPE_READ_ID 26
#define SYS_PIPE_DUMP_ID 27
#define SYS_WAIT_ID 28
#define SYS_TICKS_ELAPSED_ID 29
#define SYS_DUMP_MM_ID 30

#define SYSCALLS 31

uint64_t sysCallDispatcher(t_registers *r) {
      if (r->rax < SYSCALLS) {
            switch (r->rax) {
                  case SYS_GETMEM_ID:
                        sys_getMem(r->rdi, (uint8_t *)r->rsi);
                        break;

                  case SYS_RTC_TIME_ID:
                        return getDecimalTimeInfo((t_timeInfo)(r->rdi));
                        break;

                  case SYS_TEMP_ID:
                        return cpuTemp();
                        break;

                  case SYS_WRITE_ID:
                        fprintfBR((char *)(r->rdi), (uint8_t)(r->rsi), (t_colour)(r->rdx), (t_colour)(r->r10));
                        break;

                  case SYS_GETCHAR_ID:
                        return getchar();
                        break;

                  case SYS_CLEAR_ID:
                        clearScreen();
                        break;

                  case SYS_LOAD_APP_ID:
                        return addProcess((void (*)(int, char **))r->rdi, (int)r->rsi, (char **)r->rdx, (uint8_t)r->r10, (uint16_t*)r->r8);
                        break;

                  case SYS_INFOREG_ID:
                        return (uint64_t)getSnapshot();
                        break;

                  case SYS_PS_ID:
                        listProcesses();
                        break;

                  case SYS_SECS_ELAPSED_ID:
                        return secondsElapsed();
                        break;

                  case SYS_KILL_ID:
                        return killProcess((uint64_t)r->rdi);
                        break;

                  case SYS_NICE_ID:
                        changePriority((uint64_t)r->rdi, (uint64_t)r->rsi);
                        break;

                  case SYS_BLOCK_ID:
                        return blockProcess((uint64_t)r->rdi);
                        break;

                  case SYS_UNBLOCK_ID:
                        return unblockProcess((uint64_t)r->rdi);
                        break;

                  case SYS_GETPID_ID:
                        return currentProcessPid();
                        break;

                  case SYS_MALLOC_ID:
                        return (uint64_t)mallocBR((uint32_t)r->rdi);
                        break;

                  case SYS_FREE_ID:
                        freeBR((void *)r->rdi);
                        break;

                  case SYS_YIELD_ID:
                        yield();
                        break;

                  case SYS_SEM_OPEN_ID:
                        return sem_open((char *)r->rdi,r->rsi);
                        break;

                  case SYS_SEM_CLOSE_ID:
                        return sem_close((int)r->rdi);
                        break;

                  case SYS_SEM_POST_ID:
                        return sem_post((int)r->rdi);
                        break;

                  case SYS_SEM_WAIT_ID:
                        return sem_wait((int)r->rdi);
                        break;

                  case SYS_SEM_DUMP_ID:
                        dumpSemaphores();
                        break;

                  case SYS_PIPE_POPEN_ID:
                        return popen((char*)r->rdi);
                        break;

                  case SYS_PIPE_CLOSE_ID:
                        return closePipe((int)r->rdi);
                        break;

                  case SYS_PIPE_WRITE_ID:
                        return writePipe((int)r->rdi,(char*)r->rsi);
                        break;

                  case SYS_PIPE_READ_ID:
                        return readPipe((int)r->rdi);
                        break;

                  case SYS_PIPE_DUMP_ID:
                        dumpPipes();
                        break;

                  case SYS_WAIT_ID:
                        wait(r->rdi);
                        break;

                  case SYS_TICKS_ELAPSED_ID:
                        return ticksElapsed();
                        break;

                  case SYS_DUMP_MM_ID:
                        dumpMM();
                        break;
            }
      }
      return 0;
}