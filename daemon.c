#include <syslog.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <time.h>
#include <string.h>

#define MB 1024*1024



FILE* OpenFile(char* fName, char* dirName)
{
  char fullPath[256];
  snprintf(fullPath, sizeof(fullPath), "%s%s", "HOME/", dirName);
  chdir(getenv(fullPath));
  FILE* f;
  if (( f = fopen(fName, "ab+")) == NULL)
  {
    syslog(LOG_INFO, "Failed to create a file (# %d)!", errno);
    exit(1); 
  }
  return f;
}

void CloseFile(FILE* f)
{
  if (f != NULL) 
  {
    fclose(f);
  }
}


int GetFileSize(char* fName, char* dirName)
{
  FILE* f = OpenFile(fName, dirName);
  int size = 0;
  int currPos = ftell(f);
  fseek(f, 0, SEEK_END);
  size = ftell(f); 
  fseek(f, currPos, SEEK_SET);
  CloseFile(f);
  return size;
}

void CreateDirectory(char* dirName)
{
  chdir(getenv("HOME"));
  if (mkdir(dirName, S_IRWXU | S_IRWXO | S_IRWXG))
  {
    syslog(LOG_INFO, "Dir is already created!");    
  }
}

void randomise(int* buf, int bufSize, char* fName, char* dirName);

void WriteToFile(char* fName, char* dirName)
{
  int buf1[256];
  int buf2[256];
  int i = 0;
  while (GetFileSize(fName, dirName) < 5 * MB)
  {
    FILE* f = OpenFile(fName, dirName);        
    randomise(buf1, 256, fName, dirName);	
    fwrite(buf1, sizeof(int), 256, f);
    CloseFile(f);      
  }   
}

void sig_handler(int signo)
{
//handle signals
  if(signo == SIGTERM)
  {
    syslog(LOG_INFO, "SIGTERM has been caught! Exiting...");
    if(remove("run/daemon.pid") != 0)
    {
      syslog(LOG_ERR, "Failed to remove the pid file. Error number is %d!", errno);
      exit(1);
    }
    exit(0);
  }
  
  if(signo == SIGINT)
  {
    syslog(LOG_INFO, "SIGINT has been caught! Exiting...");
    if(remove("run/daemon.pid") != 0)
    {
      syslog(LOG_ERR, "Failed to remove the pid file. Error number is %d", errno);
      exit(1);
    }
    exit(0);
  }
}

pid_t GetPid(char* fName, char* dirName)
{
  FILE* f = OpenFile(fName, dirName);
  pid_t pid = -1;
  fscanf(f, "%d", &pid);
  CloseFile(f);
  return pid;
}

void randomise(int* buf, int bufSize, char* fName, char* dirName)
{
  int i = 0, j = 0;
  time_t rawtime; 
  int* randBuf = malloc(bufSize*sizeof(int));
  const unsigned long shift = 1000;
  unsigned long uptime = 0;
  unsigned long idle = 0;
  
  FILE* fd;
  if ((fd = OpenFile("uptime", "./../../proc/")) !=  NULL)
  {
    fscanf(fd, "%li", &uptime);
    fscanf(fd, "%li", &idle);
  }
  CloseFile(fd);

  
  for (i = 0; i < bufSize; i++)
  {
    struct sysinfo info;
    sysinfo(&info);	
    for(j = 0; j < rand() % shift; j++)
    {
      buf[i] +=(int) ((rand() ^ idle) * info.bufferram ^ rand() * (info.bufferram * uptime) ^ info.procs);
    }
  }
  free(randBuf);
  
}


void handle_signals()
{
  if((signal(SIGTERM, sig_handler) == SIG_ERR) ||
     (signal(SIGINT, sig_handler) == SIG_ERR))
  {
    syslog(LOG_ERR, "Error! Can't catch SIGTERM or SIGINT.");
    exit(1);
  }
  
}

void daemonise()
{
  pid_t pid, sid;
  FILE *pid_fp;

  syslog(LOG_INFO, "Starting daemonisation.");

  //First fork
  pid = fork();
  if(pid < 0)
  {
    syslog(LOG_ERR, "Error occured in the first fork while daemonising. Error number is %d", errno);
    exit(1);
  }

  if(pid > 0)
  {
    syslog(LOG_INFO, "First fork successful (Parent)");
    exit(0);
  }
  syslog(LOG_INFO, "First fork successful (Child)");

  //Create a new session
  sid = setsid();
  if(sid < 0) 
  {
    syslog(LOG_ERR, "Error occured in making a new session while daemonising. Error number is %d", errno);
    exit(1);
  }
  syslog(LOG_INFO, "New session was created successfuly!");

  //Second fork
  pid = fork();
  if(pid < 0)
  {
    syslog(LOG_ERR, "Error occured in the second fork while daemonising. Error number is %d", errno);
    exit(1);
  }

  if(pid > 0)
  {
    syslog(LOG_INFO, "Second fork successful (Parent)");
    exit(0);
  }
  syslog(LOG_INFO, "Second fork successful (Child)");

  pid = getpid();

  //Change working directory to Home directory
  if(chdir(getenv("HOME")) == -1)
  {
    syslog(LOG_ERR, "Failed to change working directory while daemonising. Error number is %d", errno);
    exit(1);
  }

  //Grant all permisions for all files and directories created by the daemon
  umask(0);

  //Redirect std IO
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  if(open("/dev/null",O_RDONLY) == -1)
  {
    syslog(LOG_ERR, "Failed to reopen stdin while daemonising. Error number is %d", errno);
    exit(1);
  }
  if(open("/dev/null",O_WRONLY) == -1)
  {
    syslog(LOG_ERR, "Failed to reopen stdout while daemonising. Error number is %d", errno);
    exit(1);
  }
  if(open("/dev/null",O_RDWR) == -1)
  {
    syslog(LOG_ERR, "Failed to reopen stderr while daemonising. Error number is %d", errno);
    exit(1);
  }

  //Create a pid file
  mkdir("run/", 0777);
  pid_fp = fopen("run/daemon.pid", "w");
  if(pid_fp == NULL)
  {
    syslog(LOG_ERR, "Failed to create a pid file while daemonising. Error number is %d", errno);
    exit(1);
  }
  if(fprintf(pid_fp, "%d\n", pid) < 0)
  {
    syslog(LOG_ERR, "Failed to write pid to pid file while daemonising. Error number is %d, trying to remove file", errno);
    fclose(pid_fp);
    if(remove("run/daemon.pid") != 0)
    {
      syslog(LOG_ERR, "Failed to remove pid file. Error number is %d", errno);
    }
    exit(1);
  }
  fclose(pid_fp);
}


int main(int argc, char** argv)
{
  char* dirRand = "random/";
  char* dirPid = "run/";
  char* fNameData = "random/data";
  char* fNamePid = "run/daemon.pid";

  if(argc != 2)
  {
    syslog( LOG_INFO, "ERROR: arg != 2" );
    exit(1);
  }
      
  CreateDirectory(dirPid);
  CloseFile(OpenFile(fNamePid, dirPid));
  CreateDirectory(dirRand);
  CloseFile(OpenFile(fNameData, dirRand));
  
  //handling of "start" argument
  if (!strcmp(argv[1], "start"))
  {
    if (GetFileSize(fNamePid, dirPid))
    {
      syslog( LOG_INFO, "Daemon is still running!");
      exit(0);
    }
    
    syslog(LOG_INFO, "Daemon has started.");
    daemonise();
    handle_signals();
      
    while(1)
    {
      sleep(1);
      WriteToFile(fNameData, dirRand);
    }
  }
  //handling of "stop" argument
  if (!strcmp(argv[1], "stop"))
  {
    if(GetFileSize(fNamePid, dirPid) < 1)
    {
      syslog( LOG_INFO, "Daemon is not running!");
      exit(0);
    }
    syslog( LOG_INFO, "Daemon has stopped.");    
    kill(GetPid(fNamePid, dirPid), SIGTERM);
    exit(0);
  }
  return 0;
}
