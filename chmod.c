#ifdef CS333_P5
#include "types.h"
#include "user.h"

#define MAXSTR 20

int
main(int argc, char **argv)
{
  if (argc != 3) {
    printf(1, "chmod - usage 'chmod mode path'\n");
    exit();
  }  

  int mode = atoo(argv[1]);

  if (mode > 3361 || mode < 0) {
    printf(1, "chmod - mode should be inbetween 0000 and 1777\n");
    exit();
  }
  
  int rc = chmod(argv[2], mode);

  if (rc < 0) {
    printf(1, "chmod - chmod failed\n");
  }

  exit();
}

#endif
