#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;
  
  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  
  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

#ifdef CS333_P5
void 
printMode (short type, union stat_mode_t mode) {
  if (type == 1) printf(1, "d");
  if (type == 2) printf(1, "-");
  if (type == 3) printf(1, "c");
  
  char c;
  if (mode.flags.setuid) c = 'S'; else c = 'x';

  if(mode.flags.u_r) printf(1, "r");  else printf(1, "-");
  if(mode.flags.u_w) printf(1, "w");  else printf(1, "-");
  if(mode.flags.u_x) printf(1, "%c", c); else printf(1, "-");

  if(mode.flags.g_r) printf(1, "r");  else printf(1, "-");
  if(mode.flags.g_w) printf(1, "w");  else printf(1, "-");
  if(mode.flags.g_x) printf(1, "%c", c); else printf(1, "-");

  if(mode.flags.o_r) printf(1, "r");  else printf(1, "-");
  if(mode.flags.o_w) printf(1, "w");  else printf(1, "-");
  if(mode.flags.o_x) printf(1, "%c\t", c); else printf(1, "-\t");
}
#endif

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  
  if((fd = open(path, 0)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    return;
  }
  
  if(fstat(fd, &st) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }
  
  switch(st.type){
  case T_FILE:
  case T_DEV:
#ifdef CS333_P5
    printMode(st.type, st.mode);
    printf(1, "%s\t\t%d\t%d\t%d\t%d\n", fmtname(path), st.uid, st.gid, st.ino, st.size);
#else
    printf(1, "%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
#endif

    break;
  
  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }

#ifdef CS333_P5
      printMode(st.type, st.mode);
      printf(1, "%s\t\t%d\t%d\t%d\t%d\n", fmtname(buf), st.uid, st.gid, st.ino, st.size);

#else
      printf(1, "%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
#endif

    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

#ifdef CS333_P5
  printf(1, "mode\t\tname\t\t\tuid\tgid\tinode\tsize\n");
#endif

  if(argc < 2){
    ls(".");
    exit();
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit();
}
