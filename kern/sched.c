#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Choose a user environment to run and run it.
void
sched_yield(void)
{
  struct Env *idle;

        // Implement simple round-robin scheduling.
        //
        // Search through 'envs' for an ENV_RUNNABLE environment in
        // circular fashion starting just after the env this CPU was
        // last running.  Switch to the first such environment found.
        //
        // If no envs are runnable, but the environment previously
        // running on this CPU is still ENV_RUNNING, it's okay to
        // choose that environment.
        //
        // Never choose an environment that's currently running on
        // another CPU (env_status == ENV_RUNNING). If there are
        // no runnable environments, simply drop through to the code
        // below to halt the cpu.

  // LAB 4: Your code here. -- DONE
  int startingEnv = curenv ? ENVX(curenv->env_id) % NENV : 0;
  int inc = curenv ? 1 : 0;         // if there's no running env, have to check all envs

  // Lab 4 priority scheduling challenge
  struct Env *highestEnv = NULL;
  struct Env *nextEqualEnv = NULL;  // account for envs with the same priority
  int highestPri = curenv ? curenv->priority : 100;     // Lower priority value -> higher task priority
  // -----------------------------------
  
  for (inc; inc < NENV; inc++) {
    int i = (startingEnv + inc) % NENV;  // circle around envs to serach all envs

    // Normal round-robin
    /*if (envs[i].env_status == ENV_RUNNABLE) {
      env_run(&envs[i]);
    }*/

    // Lab 4 priority scheduling challenge
    if (envs[i].env_status == ENV_RUNNABLE) { // check all runnable envs
      if (envs[i].priority < highestPri) {    // this env is the new highest priority
        highestPri = envs[i].priority;        
        highestEnv = &envs[i];
        nextEqualEnv = NULL;                  // invalidate any ties with old highest priority
      } else if (!nextEqualEnv && envs[i].priority == highestPri) { // this env is the first tie with the current highest priority
        nextEqualEnv = &envs[i];
      }
    }
    // -----------------------------------
  }

  // Normal round-robin
  /*if (curenv && curenv->env_status == ENV_RUNNING)
    env_run(curenv);*/

  // Lab 4 priority scheduling challenge
  if (highestEnv) {
    env_run(highestEnv);
  } else if (nextEqualEnv) {
    env_run(nextEqualEnv);
  } else if (curenv && curenv->env_status == ENV_RUNNING) {
    env_run(curenv);
  }
  // -----------------------------------

  // sched_halt never returns
  sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
  int i;

  // For debugging and testing purposes, if there are no runnable
  // environments in the system, then drop into the kernel monitor.
  for (i = 0; i < NENV; i++) {
    if ((envs[i].env_status == ENV_RUNNABLE ||
         envs[i].env_status == ENV_RUNNING ||
         envs[i].env_status == ENV_DYING))
      break;
  }
  if (i == NENV) {
    cprintf("No runnable environments in the system!\n");
    while (1)
      monitor(NULL);
  }

  // Mark that no environment is running on this CPU
  curenv = NULL;
  lcr3(PADDR(kern_pgdir));

  // Mark that this CPU is in the HALT state, so that when
  // timer interupts come in, we know we should re-acquire the
  // big kernel lock
  xchg(&thiscpu->cpu_status, CPU_HALTED);

  // Release the big kernel lock as if we were "leaving" the kernel
  unlock_kernel();

  // Reset stack pointer, enable interrupts and then halt.
  asm volatile (
    "movl $0, %%ebp\n"
    "movl %0, %%esp\n"
    "pushl $0\n"
    "pushl $0\n"
    "sti\n"
    "1:\n"
    "hlt\n"
    "jmp 1b\n"
    : : "a" (thiscpu->cpu_ts.ts_esp0));
}

