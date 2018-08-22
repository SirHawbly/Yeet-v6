#ifdef CS333_P5
#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  if (argc != 3) {
    printf(1, "chgrp - usage 'chgrp gid path'\n");
    exit();
  }  

  int gid = atoi(argv[1]);

  int rc = chgrp(argv[2], gid);

  if (rc < 0) {
    printf(1, "chgrp - chgrp failed\n");
  }

  exit();
}

#endif
