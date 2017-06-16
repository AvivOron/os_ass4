#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

int 
procfsisdir(struct inode *ip) {
	if(ip->type == T_DIR){
		return 1;
	}
	return 0;
}

void
procfsiunlock(struct inode *ip)
{
  if(ip == 0 || !(ip->flags & I_BUSY) || ip->ref < 1)
    panic("procfsiunlock");

  /*acquire(&icache.lock);
  ip->flags &= ~I_BUSY;
  wakeup(ip);
  release(&icache.lock);*/
}

//iread
void 
procfsiread(struct inode* dp, struct inode *ip) {
  if(ip == 0 || ip->ref < 1)
    panic("procfsiread");

  /*acquire(&icache.lock);
  while(ip->flags & I_BUSY)
    sleep(ip, &icache.lock);
  ip->flags |= I_BUSY;
  release(&icache.lock);*/

  if(!(ip->flags & I_VALID)){

    ip->flags |= I_VALID;
    //procfsiunlock(ip);
    if(ip->type == 0)
      panic("procfsiread: no type");
  }
}


// chdir > namei > namex > dirlookup > readi > device "read"
int
procfsread(struct inode *ip, char *dst, int off, int n) {
  //should return n (or less) bytes starting at offset 'off' and assign the data into dst buffer
	//where we read the data from?
	    cprintf("procfsread ---> start\n");

	return 0;
}

int
procfswrite(struct inode *ip, char *buf, int n)
{
  return 0;
}

void
procfsinit(void)
{
  devsw[PROCFS].isdir = procfsisdir;
  devsw[PROCFS].iread = procfsiread;
  devsw[PROCFS].write = procfswrite;
  devsw[PROCFS].read = procfsread;
}
