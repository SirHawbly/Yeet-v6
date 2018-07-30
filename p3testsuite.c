#ifdef CS333_P3P4

// Test program for CS333 scheduler, project 4.

#include "types.h"
#include "user.h"

#include "uproc.h"
#include "param.h"

// Round Robin Test
#define RR 1
// Free-Zombie Test
// #define FZ 1
// Test-Kids Test
// #define TK 1

// TESTS P3 FUNCTIONALITY
#ifdef FZ
void
infLoops(void)
{
  int pid, num = 8;

  while(num > 0) {

    pid = fork();

    if(pid == 0)
      while(1);
    
    num -= 1;
  }

  printf(2, "Parent is done creating children\n");
  while(1) ;

}

void
freeZombie(void){
  int i, ppid, pid[80]; 
  ppid = getpid(); 
  printf(2, "About to begin creating children\n");
  for(i = 0; i < 80; i++){
    pid[i] = fork();
    if(pid[i] < 0)
      break;
    if(pid[i] == 0){
      sleep(10000000);
      exit();
    }
  }
  if(getpid() == ppid){
    printf(1, "Forked %d times\n", i); 
    printf(1, "Show commands now\n");
    sleep(10*TPS); 
    
    for(int n = 0; n < i; n++){
      sleep(1*TPS); 
      
      kill(pid[n]); 
      sleep(100); 
      while(wait() == -1){};
      printf(1, "killed pid: %d; ", pid[n]);  
    }
  }
}
#endif

#ifdef RR 
void
print(int i)
{
  switch(i) {
    case 1:
      printf(1, "1 ");
      break;
    case 2:
      printf(1, "2 ");
      break;
    case 3:
      printf(1, "3 ");
      break;
  }
}

void
roundRobin()
{
  int p;

  for (int i = 0; i < 10; i++) {

    printf(1, "%d ", i); 

    p = fork();

    if (i == 9)
       for(;;); 

    if (p == 0) {
       
      printf(1, "%d ", i); 

      for(;;); 

    }
  }
}
#endif

int
main(int argc, char **argv)
{
  #ifdef FZ
  freeZombie();
  printf(1, "\n");
  // infLoops();
  #endif

  #ifdef RR 
  roundRobin();  
  #endif
  
  exit();
}

#endif
