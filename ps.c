
// p2-27
#ifdef CS333_P2

#include "types.h"
#include "user.h"
#include "uproc.h"

#define UPROC_MAX 20

void 
print_ticks(int time) 
{
  // prints time (in ticks) in seconds using leading zeros
  printf(1, "%d.", time        / 1000);
  printf(1, "%d",  time % 1000 / 100);
  printf(1, "%d",  time % 100  / 10);
  printf(1, "%d",  time % 10   / 1);
  printf(1, "s\t");
}

int
main(void)
{
  struct uproc *t = malloc(UPROC_MAX * sizeof(struct uproc));  
  
  int n = getprocs(UPROC_MAX, t);
  
  if (n < 0) printf(2, "ps: getprocs failed");

  printf(1, "\nPID\tName\tUID\tGID\tPPID\tETime\tCPU\tState\tSize\n"); 
  
  for (int i = 0; i < n; i++) {
   
    printf(1, "%d\t%s\t%d\t%d\t%d\t", 
            t[i].pid, t[i].name, t[i].uid, t[i].gid, t[i].ppid);
    print_ticks(t[i].elapsed_ticks);
    print_ticks(t[i].CPU_total_ticks);
    printf(1, "%s\t%d\n", t[i].state, t[i].size);

  }

  free(t);
  exit();
}

#endif
