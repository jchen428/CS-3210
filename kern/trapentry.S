/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)                                          \
        .data;															\
        .long name;														\
        .text;															\
        .globl name;            /* define global symbol for 'name' */   \
        .type name, @function;  /* symbol type is function */           \
        .align 2;               /* align function definition */         \
        name:                   /* function starts here */              \
        pushl $(num);                                                   \
        jmp _alltraps


/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)                                     \
        .data;															\
        .long name;														\
        .text;															\
        .globl name;                                                    \
        .type name, @function;                                          \
        .align 2;                                                       \
        name:                                                           \
        pushl $0;                                                       \
        pushl $(num);                                                   \
        jmp _alltraps


/*
 * Lab 3: Your code here for generating entry points for the different traps. -- DONE
 */
.data
.globl handlers
handlers:
.text
	TRAPHANDLER_NOEC(divide, T_DIVIDE)
	TRAPHANDLER_NOEC(debug, T_DEBUG)
	TRAPHANDLER_NOEC(nmi, T_NMI)
	TRAPHANDLER_NOEC(brkpt, T_BRKPT)
	TRAPHANDLER_NOEC(oflow, T_OFLOW)
	TRAPHANDLER_NOEC(bound, T_BOUND)
	TRAPHANDLER_NOEC(illop, T_ILLOP)
	TRAPHANDLER_NOEC(device, T_DEVICE)
	TRAPHANDLER(dblflt, T_DBLFLT)
	TRAPHANDLER_NOEC(coproc, 9)
	TRAPHANDLER(tss, T_TSS)
	TRAPHANDLER(segnp, T_SEGNP)
	TRAPHANDLER(stack, T_STACK)
	TRAPHANDLER(gpflt, T_GPFLT)
	TRAPHANDLER(pgflt, T_PGFLT)
	TRAPHANDLER_NOEC(res, 15)
	TRAPHANDLER_NOEC(fperr, T_FPERR)
	TRAPHANDLER(align, T_ALIGN)
	TRAPHANDLER_NOEC(mchk, T_MCHK)
	TRAPHANDLER_NOEC(simderr, T_SIMDERR)
	TRAPHANDLER_NOEC(t_syscall, T_SYSCALL)
	TRAPHANDLER_NOEC(t_default, T_DEFAULT)
	TRAPHANDLER_NOEC(timer, IRQ_OFFSET + IRQ_TIMER)
	TRAPHANDLER_NOEC(kbd, IRQ_OFFSET + IRQ_KBD)
	TRAPHANDLER_NOEC(irq2, IRQ_OFFSET + 2)
	TRAPHANDLER_NOEC(irq3, IRQ_OFFSET + 3)
	TRAPHANDLER_NOEC(serial, IRQ_OFFSET + IRQ_SERIAL)
	TRAPHANDLER_NOEC(irq5, IRQ_OFFSET + 5)
	TRAPHANDLER_NOEC(irq6, IRQ_OFFSET + 6)
	TRAPHANDLER_NOEC(spurious, IRQ_OFFSET + IRQ_SPURIOUS)
	TRAPHANDLER_NOEC(irq8, IRQ_OFFSET + 8)
	TRAPHANDLER_NOEC(irq9, IRQ_OFFSET + 9)
	TRAPHANDLER_NOEC(irq10, IRQ_OFFSET + 10)
	TRAPHANDLER_NOEC(irq11, IRQ_OFFSET + 11)
	TRAPHANDLER_NOEC(irq12, IRQ_OFFSET + 12)
	TRAPHANDLER_NOEC(irq13, IRQ_OFFSET + 13)
	TRAPHANDLER_NOEC(ide, IRQ_OFFSET + IRQ_IDE)
	TRAPHANDLER_NOEC(irq15, IRQ_OFFSET + 15)
	TRAPHANDLER_NOEC(error, IRQ_OFFSET + IRQ_ERROR)


/*
 * Lab 3: Your code here for _alltraps -- DONE
 */
_alltraps:
    pushl %ds
    pushl %es
    pushal
    
    movl $GD_KD, %eax
    movw %ax, %ds
    movw %ax, %es

    pushl %esp

    call trap
