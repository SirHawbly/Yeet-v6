
// p2-30
#ifdef CS333_P2

#include "types.h"
#include "user.h"

void 
print_ticks(int time) 
{
  // prints time (in ticks) in seconds using leading zeros
  printf(1, "%d.", time        / 1000);
  printf(1, "%d",  time % 1000 / 100);
  printf(1, "%d",  time % 100  / 10);
  printf(1, "%d",  time % 10   / 1);
  printf(1, " seconds.\t");
}

int
main(int argc, char **argv)
{

  int rc = 0;

  uint start = uptime(); 
  
  rc = fork();

  if (rc == 0) 
  {
    exec(argv[1], &argv[1]);
    printf(1, "time: exec failed, returning\n");
    exit();
  } 

  else 
  {
    wait();

    printf(1, "%s ran in ", argv[1]);  
    print_ticks(uptime() - start); 
    printf(1, "\n");
  }

  exit();
}

#endif
