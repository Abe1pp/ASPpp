#include <linux/ioctl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

#define CDRV_IOC_MAGIC 'Z'
#define ASP_CLEAR_BUF _IO(CDRV_IOC_MAGIC, 1)

#define devname "mycdrv"

int deviceMode = 1;

typedef struct Command {
  int devno;
  char operation;
  char *message;
  int size;
  int offset;
  char pos;
} Command;

std::map<int, int> devicefds;
std::vector<Command*> commands;

int parseCommand(std::string command, Command *cmd) {
  if (!cmd) return 0;

  try {  
    std::string word;
    std::istringstream iss(command);
    getline(iss, word, ' ');
    if (word != "device") 
       return 0;
    getline(iss, word, ' ');        
    cmd->devno = std::stoi(word);
    getline(iss, word, ' ');
    if (word == "c" || word == "w" || word == "r" || word == "l") 
       cmd->operation = word[0];
    else return 0;
    if (word == "w") {
       getline(iss, word,'\n');
       cmd->message = strdup(word.c_str());
       cmd->size = strlen(cmd->message);
    }
    else if (word == "r") {
       getline(iss, word, ' ');
       cmd->size = std::stoi(word);     
    }
    else if (word == "l") {
       getline(iss, word, ' ');
       if (word == "B" || word == "C" || word == "E") {
          cmd->pos = word[0];
       }      
       else return 0;
       getline(iss, word, '\n');
       cmd->offset = std::stoi(word);
    }
   }
   catch(const std::invalid_argument & e) {
     std::cout << e.what() << "\n";
     exit(1);
   }
   catch (const std::out_of_range & e) {
     std::cout << e.what() << "\n";
     exit(1);
   }

   return 1;   
}

void readCommands(const char *fname, std::vector<Command*> &commands) {
  std::ifstream inpf(fname);
  if (!inpf.is_open()) {
     std::cerr << "Could not open file " << fname << "\n";
     exit(1);
  }
  std::string line;
  while(std::getline(inpf,line)) {
      Command * cm = (Command*) malloc(sizeof(Command));
      if (parseCommand(line, cm)) {
         commands.push_back(cm);
      }
  }
  inpf.close();
}

void printCommand(Command *c) {
   std::cerr << "Command " << c->operation << "\n";
}

void executeCommand(Command *c, std::string fname) {
   int fd = -1;
   if (devicefds.find(c->devno) == devicefds.end()) {
      if (deviceMode) {
         fd = open(fname.c_str(), O_RDWR);
      }
      else { 
         fd = open(fname.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRWXU);
         if (fd == -1 && errno == EEXIST)
            fd = open(fname.c_str(), O_RDWR); 
      }
      if (fd == -1) {
         std::cerr << "File " << fname << " cannot be opened!\n";
         exit(1);
      }
      devicefds[c->devno] = fd;
   }
   else fd = devicefds[c->devno];
   std::cerr << "Using fd no " << fd << " for device " << c->devno << "\n";
   if (c->operation == 'c') {
      if (deviceMode) {
         int i = ioctl(fd, ASP_CLEAR_BUF);
         int en = errno;
         if (i == -1)
            std::cout << "[Device " << c->devno << "] " << "Error clearing the buffer : " << strerror(en) << "\n";
         else  
            std::cout << "[Device " << c->devno << "] " << "Cleared the buffer for device\n";
     }
     else {
        struct stat buf;
        int r = fstat(fd, &buf);
        if (buf.st_size > 0) {
           char *ptr = (char*)mmap(NULL, buf.st_size, PROT_WRITE, MAP_SHARED, fd, 0);
           int en = errno;
           if (ptr == MAP_FAILED) {
              std::cerr << "Failed mmapping the file " << fname << " : " << strerror(en) << "\n"; 
              exit(1);
           }
           memset(ptr, 0, buf.st_size);
           msync(ptr, buf.st_size, MS_SYNC);
           munmap(ptr, buf.st_size);
        }        
        std::cout << "[Device " << c->devno << "] " << "Cleared the buffer for device\n";
     } 
   }
   else if (c->operation == 'r') {
       char *buf = (char*)malloc(c->size + 1);
       *(buf + c->size) = '\0';   
       int n = read(fd, buf, c->size);
       int en = errno; 
       if (n >= 0)
          std::cout << "[Device " << c->devno << "] " << "Read " << n << " bytes: " << buf << "\n";
       else std::cout << "[Device " << c->devno << "] " << "Error reading for " << c->size << " bytes : " << strerror(en) << "\n";
   }
   else if (c->operation == 'w') {
       int n = write(fd,c->message,c->size);
       int en = errno; 
       if (n >= 0)
          std::cout << "[Device " << c->devno << "] " << "Wrote " << n << " bytes of " << c->message << "\n";
       else std::cout << "[Device " << c->devno << "] " << "Error writing " << c->message << " : " << strerror(en) << "\n";        
   }
   else if (c->operation == 'l') {
       int r;
       if (c->pos == 'B')
          r = lseek(fd, c->offset, SEEK_SET);
       else if (c->pos == 'C')
          r = lseek(fd, c->offset, SEEK_CUR);
       else if (c->pos == 'E')
          r = lseek(fd, c->offset, SEEK_END);
       if (r == -1)
          std::cout << "[Device " << c->devno << "] " << "Error lseeking using offset " << c->offset << "\n";
       else std::cout << "[Device " << c->devno << "] " << "Changed offset " << c->offset << " bytes wrt " << c->pos << " , curret offset=" << r << "\n";
   } 
}

void executeCommands(std::vector<Command*> &commands) {
 
   std::cerr << "Number of commands " << commands.size() << "\n";

   for(int i=0; i<commands.size(); i++) {
      std::cerr << "Executing command " << i << "\n";
      Command *c = commands[i];
      std::string dn(devname);
      std::string fname;
      if (deviceMode) {
         fname = "/dev/" + dn + std::to_string(c->devno);
      }
      else fname = "temp" + std::to_string(c->devno);
      executeCommand(c, fname); 
   } 
}


int main(int argc, char **argv) {

   if (argc < 2) {
      std::cout << "Usage " << argv[0] << " command_file_name [regular]\n";
      return 1;
   }

   if (argc == 3) {
      if (!strcmp(argv[2],"regular"))
         deviceMode = 0;
      else {
         std::cout << "Usage " << argv[0] << " command_file_name [regular]\n";
         return 1;
      }
   }
      
   readCommands(argv[1], commands);
   executeCommands(commands);   
   
   return 0;
}
