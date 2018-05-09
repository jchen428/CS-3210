// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE 80 // enough for one VGA text line


struct Command {
  const char *name;
  const char *desc;
  // return -1 to force monitor to exit
  int (*func)(int argc, char **argv, struct Trapframe * tf);
};

static struct Command commands[] = {
  { "help",      "Display this list of commands",               mon_help         },
  { "info-kern", "Display information about the kernel",        mon_infokern     },
  { "backtrace", "Display current stacktrace",                  mon_backtrace    },
  { "showmappings", "Display a range of kernel page mappings",  mon_showmappings },
  { "setflags", "Set permission flags of a page mapping",       mon_setflags     },
  { "memdump", "Dump the contents of a range of memory",        mon_memdump      },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
  int i;

  for (i = 0; i < NCOMMANDS; i++)
    cprintf("%s - %s\n", commands[i].name, commands[i].desc);
  return 0;
}

int
mon_infokern(int argc, char **argv, struct Trapframe *tf)
{
  extern char _start[], entry[], etext[], edata[], end[];

  cprintf("Special kernel symbols:\n");
  cprintf("  _start                  %08x (phys)\n", _start);
  cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
  cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
  cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
  cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
  cprintf("Kernel executable memory footprint: %dKB\n",
          ROUNDUP(end - entry, 1024) / 1024);
  return 0;
}


int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
  // Your code here. - DONE
  uint32_t ebp, eip, args[5];
  struct Eipdebuginfo eipInfo;
  
  cprintf("Stack backtrace:\n");
  ebp = read_ebp();
  
  while (ebp != 0) {
    eip = *((uint32_t *)ebp + 1);
    
    uint8_t i = 0;
    for (i; i < 5; i++) {
      args[i] = *((uint32_t *)ebp + 2 + i);
    }
    
    cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", ebp, eip, 
      args[0], args[1], args[2], args[3], args[4]);
      
    debuginfo_eip(eip, &eipInfo);
    cprintf("         %s:%d: %.*s+%d\n", eipInfo.eip_file, eipInfo.eip_line, 
      eipInfo.eip_fn_namelen, eipInfo.eip_fn_name, eip - eipInfo.eip_fn_addr);
      
    ebp = *((uint32_t *)ebp);
  }

  return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
  if (argc != 3) {
    cprintf("showmappings [start_addr] [end_addr]\n");
    return 0;
  }

  uint32_t start = strtol(argv[1], NULL, 16);
  uint32_t end = strtol(argv[2], NULL, 16);
  uint32_t curr = start;

  if (start >= end) {
    cprintf("end_addr must be greater than start_addr\n");
    return 0;
  }

  cprintf("start_addr = 0x%08x, end_addr = 0x%08x\n", start, end);
  cprintf("VA\t\t->\tPA\t\tPermissions\n");
  cprintf("--------------------------------------------------------\n");

  while (curr < end) {
    cprintf("0x%08x\t->\t", curr);
    pte_t *pte = pgdir_walk(kern_pgdir, (void *)curr, 0);

    if (!pte || !(*pte & PTE_P))
      cprintf("N/A\n");

    cprintf("0x%08x\t0x%03x\n", (uint32_t)PTE_ADDR(*pte), *pte & 0xfff);
    curr += PGSIZE;
  }

  return 0;
}

int
mon_setflags(int argc, char **argv, struct Trapframe *tf)
{
  if (argc != 3) {
    cprintf("setflags [addr] [flags]\n");
    return 0;
  }

  uint32_t va = strtol(argv[1], NULL, 16);
  uint32_t flags = strtol(argv[2], NULL, 16);
  pte_t *pte = pgdir_walk(kern_pgdir, (void *)va, 0);

  if (!pte) {
    cprintf("No page mapping at 0x%08x\n", va);
    return 0;
  }

  cprintf("Old flags: 0x%08x\n", *pte & 0xfff);

  *pte &= ~0xfff;
  *pte |= flags;

  cprintf("New flags: 0x%08x\n", *pte & 0xfff);

  return 0;
}

int
mon_memdump(int argc, char **argv, struct Trapframe *tf)
{
  if (argc != 3) {
    cprintf("memdump [start_addr] [end_addr]\n");
    return 0;
  }

  uint32_t start = strtol(argv[1], NULL, 16);
  uint32_t end = strtol(argv[2], NULL, 16);
  uint32_t curr = start;

  if (start >= end || start < 0 || end > 0xffffffff) {
    cprintf("end_addr must be greater than start_addr\n");
    return 0;
  }

  cprintf("start_addr = 0x%08x, end_addr = 0x%08x\n", start, end);
  cprintf("Address\t\t->\tValue\n");
  cprintf("---------------------------------------\n");

  while (curr < end) {
    cprintf("0x%08x\t->\t0x%08x\n", curr, *(void **)curr);
    curr++;
  }

  return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
  int argc;
  char *argv[MAXARGS];
  int i;

  // Parse the command buffer into whitespace-separated arguments
  argc = 0;
  argv[argc] = 0;
  while (1) {
    // gobble whitespace
    while (*buf && strchr(WHITESPACE, *buf))
      *buf++ = 0;
    if (*buf == 0)
      break;

    // save and scan past next arg
    if (argc == MAXARGS-1) {
      cprintf("Too many arguments (max %d)\n", MAXARGS);
      return 0;
    }
    argv[argc++] = buf;
    while (*buf && !strchr(WHITESPACE, *buf))
      buf++;
  }
  argv[argc] = 0;

  // Lookup and invoke the command
  if (argc == 0)
    return 0;
  for (i = 0; i < NCOMMANDS; i++)
    if (strcmp(argv[0], commands[i].name) == 0)
      return commands[i].func(argc, argv, tf);
  cprintf("Unknown command '%s'\n", argv[0]);
  return 0;
}

void
monitor(struct Trapframe *tf)
{
  char *buf;

  cprintf("Welcome to the JOS kernel monitor!\n");
  cprintf("Type 'help' for a list of commands.\n");

  if (tf != NULL)
    print_trapframe(tf);

  while (1) {
    buf = readline("K> ");
    if (buf != NULL)
      if (runcmd(buf, tf) < 0)
        break;
  }
}
