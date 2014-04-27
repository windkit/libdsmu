#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <ucontext.h>

#define TRAPNO_PAGE_FAULT 14
#define REG_ERR 19

static struct sigaction oldact;

void pf_sighandler(int sig, siginfo_t *info, ucontext_t *ctx) {

  if (sig == SIGSEGV) {
    printf("Got signal %d, fault_loc is %p\n", sig, info->si_addr);
    printf("%llu\n", ctx->uc_mcontext.gregs[19]);

    if (ctx->uc_mcontext.gregs[REG_ERR] & 0x2) {
      printf("WRITE FAULT!\n");
    } else {
      printf("READ FAULT!\n");
    }

    // Fix cause of fault (let read and write).
    if (info->si_addr == (void *)0x12340990) {
      if (mprotect((void *)0x12340000, 0x4000, (PROT_READ|PROT_WRITE)) == 0) {
	return;
      }
    }
  }

  printf("reverting to old handler\n");
  (oldact.sa_handler)(sig);
}

int main(void) {
  struct sigaction sa;

  // Register page fault handler.
  sa.sa_sigaction = (void *)pf_sighandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  if (sigaction(SIGSEGV, &sa, &oldact) != 0) {
    fprintf(stderr, "sigaction failed\n");
  }

  // Setup test memory area.
  int zero_fd = open("/dev/zero", O_RDONLY, 0644);
  void *p = mmap((void *)0x12340000, 0x4000, (PROT_NONE),
                 (MAP_ANON|MAP_PRIVATE), zero_fd, 0);
  if (p < 0) {
    fprintf(stderr, "mmap failed.\n");
    return 1;
  } else {
    printf("mmap succeeded.\n");
  }

  // Trigger a page fault.
  printf("Will try to read %p\n", ((volatile int *)p) + 612);
  ((volatile int *)p)[612] = 3;
  //printf("%d\n", ((volatile int *)p)[612]);


  // Dereference (void*)0 (null pointer) -- should give a real segfault.
  int *f = 0;
  //printf("f: %d\n", *f);
  *f = 10;

  printf("we done.\n");

  return 0;
}
