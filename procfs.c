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
#include "procfs.h"
#include "bio.h"



int writeBytes(int bytes_remaining, int bytes_written, 
	int off, int entry_count, int n, int temp_n, char * dst, char output[1024]){
		//write remaining bytes to output
		if (off < ((entry_count) * sizeof(struct dirent))){
			bytes_remaining = (entry_count * sizeof(struct dirent)) - off;
			if (bytes_remaining < n)
				temp_n = bytes_remaining;
			memmove(dst, output + off, temp_n);
			bytes_written = temp_n;
			return bytes_written;
		}
		return 0;
}

int writeToN(int bytes_remaining, 
	int off, int temp_n, char * dst, char buffer[256]){
      if (off < strlen(buffer)){
        bytes_remaining = strlen(buffer) - off;
        if (bytes_remaining < temp_n){
          temp_n = bytes_remaining;
        }
        // Write to output buffer
        memmove(dst, buffer + off, temp_n);
        return temp_n;
      }
      return 0;
}

// if the file is not a directory return 0, else return num != 0
int 
procfsisdir(struct inode *ip) {
	//make sure is a directory and is under procfs
	if(ip->type == T_DEV  && ip->major==PROCFS 
    && ip->inum == namei("/proc")->inum){
  		return 1;
  }
  else{
    return ip->minor == DIRECTORY;
  }
}

//iread
//updates ip fields, if ip->flags doesn't have I_VALID, the inode will be read from disk
//all elements in procfs are virtual so they will not be read from disk
//ip->inum is initialized by iget
void 
procfsiread(struct inode* dp, struct inode *ip) {
	//check if is a directory / file etc and initialize accordingly
	//initiate the /proc directory
    if (ip->inum == namei("/proc")->inum){
      //all fields identical to parent directory
      ip->ref = dp->ref;  //same references
      ip->size = dp->size;
      ip->type = dp->type;
      ip->nlink = dp->nlink;
      ip->dev = dp->dev; //same device  

   		ip->major = PROCFS;
    	//use minor to indicate this is a directory
	    ip->minor = DIRECTORY;
	    ip->flags = dp->flags | I_VALID; //initiate valid flag

      return;
  } 

  // intiate file
  if ((ip->inum >= 3000 && ip->inum <= 3001) || //range of FILES IN PROC - BLOCKSTAT / INODESTAT
          (ip->inum >= 2000 && ip->inum < 3000) || // range of STATUS
          (ip->inum >= 10000 && ip->inum <= 20000)){ // range of FDs
    //same as parent
    ip->dev = dp->dev;
    ip->ref = dp->ref;
    ip->size = dp->size;
    ip->nlink = dp->nlink;

    //use minor to indicate this is a directory
    ip->major = PROCFS;
    ip->minor = FILE;
    ip->flags = ip->flags | I_VALID;
    ip->type = T_DEV; 

    return;
  } 

  //intiate device as a directory
  if ((ip->inum >= 1000 && ip->inum < 2000) || // range of PID
           (ip->inum >= 4000 && ip->inum <= 5000)){ // range of FDINFO
    //same as parent
    ip->dev = dp->dev;
    ip->ref = dp->ref;
    ip->size = dp->size;
    ip->nlink = dp->nlink;

    //use minor to indicate this is a directory
    ip->major = PROCFS;
    ip->minor = DIRECTORY;
    ip->flags = ip->flags | I_VALID;
    ip->type = T_DEV; //to be a device 
  } 
}

// chdir > namei > namex > dirlookup > readi > device "read"
int
procfsread(struct inode *ip, char *dst, int off, int n) {
	int num_of_default_dirents = 4;
	int wCount = 0; 
	int remCount = 0; 
	int tempCount = n; 
	int i, pid;
	// dirent array for virtual folders
	struct dirent dirent_entries[NPROC+4]; 
	struct proc p;
	//buffer
	char output[1000];

	//intiate PROC
	//check that inum (inode number) is equal the /proc directory
	if (ip->inum == namei("/proc")->inum){
		//proc intiation
		strcpy(dirent_entries[0].name, "."); 
		dirent_entries[0].inum = namei("/proc")->inum; 
		memmove(output, &dirent_entries[0], sizeof(struct dirent));
		strcpy(dirent_entries[1].name,".."); 
		dirent_entries[1].inum = 1; 
		memmove(output+sizeof(struct dirent), &dirent_entries[1], sizeof(struct dirent));
    	strcpy(dirent_entries[2].name,"blockstat"); 
    	dirent_entries[2].inum = 3000; 
    	memmove(output+(2 * sizeof(struct dirent)), &dirent_entries[2], sizeof(struct dirent));
	    strcpy(dirent_entries[3].name,"inodestat"); 
    	dirent_entries[3].inum = 3001; 
    	memmove(output+(3 * sizeof(struct dirent)), &dirent_entries[3], sizeof(struct dirent));

		//for each process running, create a directory (this is done in runtime)
		for(i = 0; i < NPROC; i++){
			p = findProc(i);
			//if process is alive
			if ((p.state != ZOMBIE) && (p.state != UNUSED)){
				numToCHar(p.pid, dirent_entries[num_of_default_dirents].name);
				//start PIDs from 1000
				dirent_entries[num_of_default_dirents].inum = p.pid + 1000;
				memmove(output+(num_of_default_dirents * sizeof(struct dirent)), &dirent_entries[num_of_default_dirents], sizeof(struct dirent));
				num_of_default_dirents++;
			}
		}
		return writeBytes(remCount, wCount, off, num_of_default_dirents, n, tempCount, dst, output);
	}
	//initiate processes
	else if ((ip->type == T_DEV) && (ip->major == PROCFS)){

    //proc root files
    if(ip->inum == 3000){ //blockstat
      char blockstat_buff[256];
      strcpy(blockstat_buff, "");
      strcpy(blockstat_buff,"Free blocks: ");
      char numOfFreeBlocksStr[10] = "";
      numToCHar(getFreeBlocks(), numOfFreeBlocksStr);
      strcpy(blockstat_buff+strlen(blockstat_buff),numOfFreeBlocksStr);
      strcpy(blockstat_buff+strlen(blockstat_buff),"\n");

      strcpy(blockstat_buff+strlen(blockstat_buff),"Total blocks: ");
      char numOfTotalBlocksStr[10] = "";
      numToCHar(getTotalBlocks(), numOfTotalBlocksStr);
      strcpy(blockstat_buff+strlen(blockstat_buff),numOfTotalBlocksStr);
      strcpy(blockstat_buff+strlen(blockstat_buff),"\n");

      strcpy(blockstat_buff+strlen(blockstat_buff),"Hit ratio: ");
      char hitRatio[10] = "";
      numToCHar(getNumOfBlockHits(), hitRatio);
      strcpy(blockstat_buff+strlen(blockstat_buff),hitRatio);
      strcpy(blockstat_buff+strlen(blockstat_buff),"\\");
      numToCHar(getNumOfBlockAccess(), hitRatio);
      strcpy(blockstat_buff+strlen(blockstat_buff),hitRatio);

      strcpy(blockstat_buff+strlen(blockstat_buff),"\n");


      return writeToN(remCount, off, tempCount, dst, blockstat_buff);

    }
    else if(ip->inum == 3001){
      char inodestat_buff[256];
      strcpy(inodestat_buff, "");
      strcpy(inodestat_buff,"Free inodes: ");
      char buf[10] = "";
      numToCHar(getFreeInodes(), buf);
      strcpy(inodestat_buff+strlen(inodestat_buff),buf);
      strcpy(inodestat_buff+strlen(inodestat_buff),"\n");

      strcpy(inodestat_buff+strlen(inodestat_buff),"Valid inodes: ");
      numToCHar(getValidInodes(), buf);
      strcpy(inodestat_buff+strlen(inodestat_buff),buf);
      strcpy(inodestat_buff+strlen(inodestat_buff),"\n");

      strcpy(inodestat_buff+strlen(inodestat_buff),"Refs per inode: ");
      numToCHar(getTotalRefs(), buf);
      strcpy(inodestat_buff+strlen(inodestat_buff),buf);
      strcpy(inodestat_buff+strlen(inodestat_buff),"\\");
      numToCHar(getUsedInodes(), buf);
      strcpy(inodestat_buff+strlen(inodestat_buff),buf);
      strcpy(inodestat_buff+strlen(inodestat_buff),"\n");


      // Write to dst
      return writeToN(remCount, off, tempCount, dst, inodestat_buff);


    }
    else if (ip->inum >= 1000 && ip->inum < 2000){ //PID
	      strcpy(dirent_entries[0].name, "."); 
	      dirent_entries[0].inum = ip->inum; 
	      memmove(output, &dirent_entries[0], sizeof(struct dirent));

	      strcpy(dirent_entries[1].name,"..");
	      dirent_entries[1].inum = namei("/proc")->inum; 
	      memmove(output+sizeof(struct dirent), &dirent_entries[1], sizeof(struct dirent));
	      
	      //CWD
	      pid = ip->inum % 1000;
	      struct inode* cwd = 0;
	      for(i = 0; i < NPROC; i++){
	        p = findProc(i);
	        if (p.pid == pid){
	          cwd = p.cwd;
	          break;
	        }
	      }

	      //NO PROCESS
	      if (cwd == 0)
	      	return 0;

	      //FOUND PROCESS
	      //CWD
	      strcpy(dirent_entries[2].name,"cwd");
	      if (cwd)
	        dirent_entries[2].inum = cwd->inum;
	      else 
	        dirent_entries[2].inum = 4000 + ip->inum;
	      memmove(output+(2 * sizeof(struct dirent)), &dirent_entries[2], sizeof(struct dirent));
	      //FDINFO
	      strcpy(dirent_entries[3].name,"fdinfo");
	      dirent_entries[3].inum = 3000 + ip->inum;
	      memmove(output+(3 * sizeof(struct dirent)), &dirent_entries[3], sizeof(struct dirent));
	      //STATUS
	      strcpy(dirent_entries[4].name,"status");
	      dirent_entries[4].inum = 1000 + ip->inum;
	      memmove(output+(4 * sizeof(struct dirent)), &dirent_entries[4], sizeof(struct dirent));

	      //write content to dst
	      return writeBytes(remCount, wCount, off, 5, n, tempCount, dst, output);


	    }  //STATUS
    	else if (ip->inum >= 2000 && ip->inum < 3000){
    		char buffer[150];
    		int pid = ip->inum % 1000;
		    for(i = 0; i < NPROC; i++){
		    	p = findProc(i);
		        if (p.pid == pid)
		          break;
		    }

	  //NO PID
      if (i == NPROC)
      	return 0;

      //PID FOUND
      strcpy(buffer, "");
      strcpy(buffer, "Run State: ");
      switch (p.state){
       case RUNNABLE:
          strcpy(buffer+strlen(buffer), "RUNNABLE");
          break;
      case RUNNING:
          strcpy(buffer+strlen(buffer), "RUNNING");
          break;  
        case SLEEPING:
          strcpy(buffer+strlen(buffer), "SLEEPING");
          break;      
      case UNUSED:
            strcpy(buffer+strlen(buffer), "UNUSED");
            break;
        case EMBRYO:
          strcpy(buffer+strlen(buffer), "EMBRYO");
          break;  
       case ZOMBIE:
          strcpy(buffer+strlen(buffer), "ZOMBIE");
          break;
        default:
          strcpy(buffer+strlen(buffer), "NULL");
          break;
      }
      strcpy(buffer+strlen(buffer), ", Memory Usage: ");
      numToCHar(p.sz, buffer+strlen(buffer));
      strcpy(buffer+strlen(buffer), "\n");
      tempCount = n;
      // Write to dst
      return writeToN(remCount, off, tempCount, dst, buffer);

    }
	//FDINFO
    else if (ip->inum >= 4000 && ip->inum < 5000){
      char fdinfo_buff[1024]; 
      int pid = ip->inum % 1000;
      int fd_written = 0; 
      struct dirent fd_direntry; 

      for(i = 0; i < NPROC; i++){
        p = findProc(i);
        if (p.pid == pid)
          break;
      }

	  //NO PID
      if (i == NPROC)
      	return 0;

      //PID FOUND
      struct file **fsd;
      fsd = p.ofile;

      fd_direntry.inum = ip->inum;
      strcpy(fd_direntry.name, ".");
      memmove(fdinfo_buff+(sizeof(struct dirent)*0), (char*)&fd_direntry, sizeof(struct dirent));
      fd_direntry.inum = ip->inum % 1000 + 1000;
      strcpy(fd_direntry.name, "..");
      memmove(fdinfo_buff+(sizeof(struct dirent)*1), (char*)&fd_direntry, sizeof(struct dirent));
      fd_written = PROCFS;

      //iterate file descriptors
      for (i = 0; i < NOFILE; i++){
        if (fsd[i]->ref > 0){
          //write to buffer
          numToCHar(i, fd_direntry.name);
          fd_direntry.inum = pid + ((i + 10) * 1000);
          memmove(fdinfo_buff+(sizeof(struct dirent)*fd_written), (char*)&fd_direntry, sizeof(struct dirent));
          fd_written++;
        }
      }
      tempCount = n;

      // Write to dst - special function for fdinfo - since size is critical by fd_written
      if (off <  fd_written * sizeof(struct dirent)){
        remCount =  fd_written * sizeof(struct dirent) - off;
        if (remCount < tempCount)
          tempCount = remCount;
        memmove(dst, fdinfo_buff + off, tempCount);
        return tempCount;
      }
    }
    //FDINFO file descriptor 
    else if (ip->inum >= 10000 && ip->inum < 20000){
      int fd = ((int)ip->inum/1000)-10;
      char fd_buffer[256];
      int pid = ip->inum % 1000;

      for(i = 0; i < NPROC; i++){
        p = findProc(i);
        if (p.pid == pid)
          break;
      }

	  //NO PID
      if (i == NPROC)
      	return 0;

      //PID FOUND 
      strcpy(fd_buffer, "FD: ");
      numToCHar(fd, fd_buffer+strlen(fd_buffer));
      strcpy(fd_buffer+strlen(fd_buffer), " REF: ");
      numToCHar(p.ofile[fd]->ref, fd_buffer+strlen(fd_buffer));
      strcpy(fd_buffer+strlen(fd_buffer), " INODE_NUM: ");
      numToCHar(p.ofile[fd]->ip->inum, fd_buffer+strlen(fd_buffer));
      strcpy(fd_buffer+strlen(fd_buffer), " TYPE: ");
      switch(p.ofile[fd]->type){
        case (FD_NONE):
          strcpy(fd_buffer+strlen(fd_buffer), "FD_NONE");
          break;
       case (FD_PIPE):
        strcpy(fd_buffer+strlen(fd_buffer), "FD_PIPE");
        break;
       case (FD_INODE):
        strcpy(fd_buffer+strlen(fd_buffer), "FD_INODE");
        break;
       default:
        strcpy(fd_buffer+strlen(fd_buffer), "DEFAULT");
        break;
      }
      strcpy(fd_buffer+strlen(fd_buffer), " POSITION: ");
      numToCHar(p.ofile[fd]->off, fd_buffer+strlen(fd_buffer));

      strcpy(fd_buffer+strlen(fd_buffer), " FLAGS: ");
      if(p.ofile[fd]->readable){
        strcpy(fd_buffer+strlen(fd_buffer), "R"); 
      }
      if(p.ofile[fd]->writable){
        strcpy(fd_buffer+strlen(fd_buffer), "W "); 
      }

      tempCount = n;

      //Write to dst
		return writeToN(remCount, off, tempCount, dst, fd_buffer);
      
    	}
    }	
	return 0;
}

int
procfswrite(struct inode *ip, char *buf, int n)
{
  cprintf("procfs is a read-only file system - canno't write to inode %d*\n", ip->inum);
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
