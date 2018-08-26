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

  if (uid > 32767 || uid < 0) {
    printf(1, "chown - user id value needs to be in between 0 and 32767\n");
    exit();
  }

  int rc = chown(argv[2], uid);

  if (rc < 0) {
    printf(1, "chown - chmod failed\n");
  }

  exit();
}

#endif
