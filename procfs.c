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




// if the file is not a directory return 0, else return num != 0
int 
procfsisdir(struct inode *ip) {
	//make sure is a durectory and is under procfs
	if(ip->type == T_DIR  && ip->major==PROCFS){
		//check that inum (inode number) is equal to a number in the /proc directory
		if (ip->inum == namei("/proc")->inum)
        	return 1;
      	else
        	return ip->minor == DIRECTORY;
	}
     return ip->minor == DIRECTORY;
}

//iread
//updates ip fields, if ip->flags doesn't have I_VALID, the inode will be read from disk
//all elements in procfs are virtual so they will not be read from disk
//ip->inum is initialized by iget
void 
procfsiread(struct inode* dp, struct inode *ip) {
	//check if is a directory / file etc and initialize accordingly
	//check that inum (inode number) is equal to a number in the /proc directory

	//initiate directory
    if (ip->inum == namei("/proc")->inum){
    	//use minor to indicate this is a directory
	    ip->minor = DIRECTORY;
	    ip->major = PROCFS;
	    ip->dev = dp->dev; //same device
	    ip->ref = dp->ref;
	    ip->flags = dp->flags | I_VALID; //initiate valid flag
	    ip->type = dp->type;
	    ip->nlink = dp->nlink;
	    ip->size = dp->size;
  }// intiate file
  else if ((ip->inum >= 3000 && ip->inum < 4000) || // cmdline
          (ip->inum >= 6000 && ip->inum <= 7000) || // status
          (ip->inum >= 10000 && ip->inum <= 16000)){ // file descriptors
    ip->minor = FILE;
    ip->major = PROCFS;
    ip->flags = ip->flags | I_VALID;

    ip->dev = dp->dev;
    ip->ref = dp->ref;
    ip->type = T_DEV; 
    ip->nlink = dp->nlink;
    ip->size = dp->size;
  }//intiate device as a directory
  else if ((ip->inum >= 1000 && ip->inum <= 2000) || // PID
           (ip->inum >= 5000 && ip->inum <= 6000)){ // fdinfo
  	//use minor to indicate this is a directory
    ip->minor = DIRECTORY;
    ip->major = PROCFS;
    ip->flags = ip->flags | I_VALID;
    ip->dev = dp->dev;
    ip->ref = dp->ref;
    ip->type = T_DEV; 
    ip->nlink = dp->nlink;
    ip->size = dp->size;
  }
}




// chdir > namei > namex > dirlookup > readi > device "read"
int
procfsread(struct inode *ip, char *dst, int off, int n) {
	cprintf("i'm in procfsread\n");
	// dirent array for virtual folders
	struct dirent dirent_entries[NPROC+2]; //ref: entries

	// create dir array
	struct proc p;
	char output[1024]; //ref: out

	int entry_count = 2; //ref: ent_count
	int bytes_written = 0; //ref: written_bytes
	int bytes_remaining = 0; //ref: remaining
	int temp_n = n; //ref: actual_n

	int i, pid;

	//intiate PROC
	//check that inum (inode number) is equal to a number in the /proc directory
	if (ip->inum == namei("/proc")->inum){
		//proc intiation
		strcpy(dirent_entries[0].name, "."); 
		dirent_entries[0].inum = namei("/proc")->inum; 
		memmove(output, &dirent_entries[0], sizeof(struct dirent));

		strcpy(dirent_entries[1].name,".."); 
		dirent_entries[1].inum = 1; 
		memmove(output+sizeof(struct dirent), &dirent_entries[1], sizeof(struct dirent));

		//for each process running, create a directory (this is done in runtime)
		for(i = 0; i < NPROC; i++){
			p = getProc(i);
			//if process is alive
			if ((p.state != ZOMBIE) && (p.state != UNUSED)){
				itoa(p.pid, dirent_entries[entry_count].name);
				//start PIDs from 1000
				dirent_entries[entry_count].inum = p.pid + 1000;
				memmove(output+(entry_count * sizeof(struct dirent)), &dirent_entries[entry_count], sizeof(struct dirent));
				entry_count++;
			}
		}

		//write remaining bytes to output
		if (off < ((entry_count) * sizeof(struct dirent))){
			bytes_remaining = (entry_count * sizeof(struct dirent)) - off;
			if (bytes_remaining < n)
				temp_n = bytes_remaining;

			memmove(dst, output + off, temp_n);
			bytes_written = temp_n;
			return bytes_written;
		}
	}
	//initiate processes
	else if ((ip->type == T_DEV) && (ip->major == PROCFS)){
		//PID
	    if (ip->inum >= 1000 && ip->inum < 2000){
	      strcpy(dirent_entries[0].name, "."); 
	      dirent_entries[0].inum = ip->inum; 
	      memmove(output, &dirent_entries[0], sizeof(struct dirent));

	      strcpy(dirent_entries[1].name,"..");
	      dirent_entries[1].inum = namei("/proc")->inum; 
	      memmove(output+sizeof(struct dirent), &dirent_entries[1], sizeof(struct dirent));
	      
	      //CMDLINE
	      strcpy(dirent_entries[2].name,"cmdline");
	      dirent_entries[2].inum = 2000 + ip->inum;
	      memmove(output+(2 * sizeof(struct dirent)), &dirent_entries[2], sizeof(struct dirent));

	      //CWD
	      pid = ip->inum % 1000;
	      struct inode* cwd = 0;
	      for(i = 0; i < NPROC; i++){
	        p = getProc(i);

	        if (p.pid == pid){
	          cwd = p.cwd;
	          break;
	        }
	      }

	      //NO PROCESS
	      if (cwd == 0)
	      	return 0;

	      //FOUND PROCESS
	      strcpy(dirent_entries[3].name,"cwd");

	      if (cwd)
	        dirent_entries[3].inum = cwd->inum;
	      else 
	        dirent_entries[3].inum = 3000 + ip->inum;
	      
	      memmove(output+(3 * sizeof(struct dirent)), &dirent_entries[3], sizeof(struct dirent));

	      //FDINFO
	      strcpy(dirent_entries[5].name,"fdinfo");
	      dirent_entries[5].inum = 4000 + ip->inum;
	      memmove(output+(5 * sizeof(struct dirent)), &dirent_entries[5], sizeof(struct dirent));

	      //STATUS
	      strcpy(dirent_entries[6].name,"status");
	      dirent_entries[6].inum = 5000 + ip->inum;
	      memmove(output+(6 * sizeof(struct dirent)), &dirent_entries[6], sizeof(struct dirent));

	      //write content to dst
	      if (off < ((7) * sizeof(struct dirent))){
	        bytes_remaining = (7 * sizeof(struct dirent)) - off;
	        //cprintf("*actual_n==%d\n", actual_n);

	        if (bytes_remaining < n){
	          temp_n = bytes_remaining;
	        }

	        // Write to output buffer
	        memmove(dst, output + off, temp_n);

	        bytes_written = temp_n;

	        return bytes_written;
	      }
	    }
	//FDINFO
    else if (ip->inum >= 5000 && ip->inum < 6000){
      char fdinfo_buff[1024]; //ref: dfinfo_buffer
      int pid = ip->inum % 1000;
      int fd_written = 0; //written_fds
      struct dirent fd_direntry; //fd_dirent

      for(i = 0; i < NPROC; i++){
        p = getProc(i);
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
          itoa(i, fd_direntry.name);
          fd_direntry.inum = pid + ((i + 10) * 1000);
          memmove(fdinfo_buff+(sizeof(struct dirent)*fd_written), (char*)&fd_direntry, sizeof(struct dirent));
          fd_written++;
        }
      }
      temp_n = n;

      // Write to dst
      if (off <  fd_written * sizeof(struct dirent)){
        bytes_remaining =  fd_written * sizeof(struct dirent) - off;
        if (bytes_remaining < temp_n)
          temp_n = bytes_remaining;

        memmove(dst, fdinfo_buff + off, temp_n);

        return temp_n;
      }
    }
    //STATUS //ref: didn't change yet
    else if (ip->inum >= 6000 && ip->inum < 7000){
      char status_buff[256];
      int pid = ip->inum % 1000;
      for(i = 0; i < NPROC; i++){
        p = getProc(i);
        if (p.pid == pid)
          break;
      }

	  //NO PID
      if (i == NPROC)
      	return 0;

      //PID FOUND
      strcpy(status_buff, "");
      strcpy(status_buff, "Process run state:: ");
      switch (p.state){
        case UNUSED:
            strcpy(status_buff+strlen(status_buff), "UNUSED");
            break;
        case EMBRYO:
          strcpy(status_buff+strlen(status_buff), "EMBRYO");
          break;        
        case SLEEPING:
          strcpy(status_buff+strlen(status_buff), "SLEEPING");
          break;
       case RUNNABLE:
          strcpy(status_buff+strlen(status_buff), "RUNNABLE");
          break;
      case RUNNING:
          strcpy(status_buff+strlen(status_buff), "RUNNING");
          break;
       case ZOMBIE:
          strcpy(status_buff+strlen(status_buff), "ZOMBIE");
          break;
        default:
          strcpy(status_buff+strlen(status_buff), "default");
          break;
      }
      
      strcpy(status_buff+strlen(status_buff), ", Memory usage:: ");
      itoa(p.sz, status_buff+strlen(status_buff));
      strcpy(status_buff+strlen(status_buff), "\n");
      temp_n = n;

      // Write to dst
      if (off < strlen(status_buff)){
        bytes_remaining = strlen(status_buff) - off;
        //cprintf("*actual_n==%d\n", actual_n);

        if (bytes_remaining < temp_n){
          temp_n = bytes_remaining;
        }
        // Write to output buffer
        memmove(dst, status_buff + off, temp_n);

        return temp_n;
      }
    }
    //FDINFO file descriptor //ref: didn't change internal params
    else if (ip->inum >= 10000 && ip->inum < 16000){
      int fd = ((int)ip->inum/1000)-10;
      char fd_buff[256];
      int pid = ip->inum % 1000;

      for(i = 0; i < NPROC; i++){
        p = getProc(i);
        if (p.pid == pid)
          break;
      }

	  //NO PID
      if (i == NPROC)
      	return 0;

      //PID FOUND
      strcpy(fd_buff, "TYPE = ");
      switch(p.ofile[fd]->type){
        case (FD_NONE):
                strcpy(fd_buff+strlen(fd_buff), "FD_NONE");
                break;
       case (FD_PIPE):
                strcpy(fd_buff+strlen(fd_buff), "FD_PIPE");
                break;
       case (FD_INODE):
                strcpy(fd_buff+strlen(fd_buff), "FD_INODE");
                break;
       default:
                strcpy(fd_buff+strlen(fd_buff), "DEFAULT");
                break;
      }
      strcpy(fd_buff+strlen(fd_buff), "; OFFSET = ");
      itoa(p.ofile[fd]->off, fd_buff+strlen(fd_buff));

      strcpy(fd_buff+strlen(fd_buff), "; WRITABLE = ");
      itoa(p.ofile[fd]->writable, fd_buff+strlen(fd_buff));

      strcpy(fd_buff+strlen(fd_buff), "; READABLE = ");
      itoa(p.ofile[fd]->readable, fd_buff+strlen(fd_buff));

      strcpy(fd_buff+strlen(fd_buff), ";\n"); 


      temp_n = n;

      //Write to dst
      if (off < strlen(fd_buff)){
        bytes_remaining = strlen(fd_buff) - off;
        if (bytes_remaining < temp_n)
          temp_n = bytes_remaining;
        
        // Write to output buffer
        memmove(dst, fd_buff + off, temp_n);
        return temp_n;
      }
    }
    }	
	return 0;
}

int
procfswrite(struct inode *ip, char *buf, int n)
{
  cprintf("procfs is a read-only file system - canno't write to inum=%d*\n", ip->inum);
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
