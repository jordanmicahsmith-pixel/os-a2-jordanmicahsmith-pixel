#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/pinfo.h"

#define NPROC 64

static struct pinfo tab[NPROC];
static int fails;

static void
check(int ok, char *msg)
{
  if(ok)
    printf("ok   %s\n", msg);
  else {
    printf("FAIL %s\n", msg);
    fails++;
  }
}

static int
lookup(int pid, struct pinfo *out)
{
  int n = getpinfo(tab, NPROC);
  for(int i = 0; i < n; i++){
    if(tab[i].pid == pid){
      *out = tab[i];
      return 0;
    }
  }
  return -1;
}

int
main(void)
{
  struct pinfo a, b, c, k;
  int me = getpid();

  // getpinfo argument handling
  check(getpinfo(tab, -1) == -1, "max < 0 returns -1");
  check(getpinfo(tab, 0) == 0, "max == 0 returns 0");
  check(getpinfo(tab, 1) == 1, "max == 1 returns 1");
  check(getpinfo((struct pinfo*)(1L << 40), 1) == -1, "bad pointer returns -1");
  check(lookup(me, &a) == 0, "own pid appears in table");

  // a lone spinner should be charged about one tick per tick
  int t0 = uptime();
  while(uptime() == t0)
    ;
  t0 = uptime();
  lookup(me, &a);
  while(uptime() < t0 + 20)
    ;
  lookup(me, &b);
  int d = b.runticks - a.runticks;
  printf("     spun 20 ticks, charged %d (level %d)\n", d, b.level);
  check(d >= 16 && d <= 22, "spinner charged roughly every tick");

  // nsched grows once we sleep and get scheduled again
  pause(2);
  lookup(me, &c);
  printf("     nsched %d -> %d\n", b.nsched, c.nsched);
  check(c.nsched > b.nsched, "nsched grows");

  // a child must not inherit counters
  int pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }
  if(pid == 0){
    pause(5);
    exit(0);
  }
  int r = lookup(pid, &k);
  check(r == 0, "child appears in table");
  if(r == 0){
    printf("     parent runticks %d, child runticks %d nsched %d\n",
           c.runticks, k.runticks, k.nsched);
    check(k.runticks <= 1 && k.nsched <= 1, "child counters start near 0");
    check(k.ctime >= c.ctime, "child ctime is recent");
  }
  wait(0);

  printf(fails ? "pinfotest: %d FAILED\n" : "pinfotest: all passed\n", fails);
  exit(fails ? 1 : 0);
}