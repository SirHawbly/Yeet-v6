#ifdef CS333_P5
#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  if (argc != 3) {
    printf(1, "chown - usage 'chown uid path'\n");
    exit();
  }  

  int uid = atoi(argv[1]);

  int rc = chown(argv[2], uid);

  if (rc < 0) {
    printf(1, "chown - chmod failed\n");
  }

  exit();
}

#endif
