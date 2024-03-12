#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h> //replace tlpi_hdr.h
#include <sys/wait.h>

typedef struct Tuple{
	char userID[5] ;   
	char topic[16]; 
	int score;
	} Tuple;
	
	
typedef struct Buffer{
	char **Tuples;
	// int count;
	sem_t buf_mutex,empty_count,fill_count;
	int inSlotIndex;
	int outSlotIndex;
} Buffer;

Buffer *buffer;


typedef struct TopicTotal{
      char userID[20];
      char topic[20];
      int score; 
}Topictotal;

struct TopicTotal topictotal[50];

int numOfSlots, numOfReducers, str_idx, size;
int *done;
char** userToRdrMap = {NULL};


//action score translator for mapper
int assScore(char action){ 
	switch(action){
		case 'P': 
			return 50;
		case 'L': 
			return 20;
		case 'D':
			return -10;
		case 'C':
			return 30;
		case 'S':
			return 40;
		default:
			return 0;
	}
}


void addToUser (char *userId) {
    char *copy = NULL;
    if (str_idx >= numOfReducers) {
        numOfReducers = numOfReducers * 2;
        userToRdrMap = realloc(userToRdrMap, numOfReducers * sizeof(char *));
    }
    copy = malloc(strlen(userId) * sizeof(char));
    copy = strcpy(copy, userId);
    userToRdrMap[str_idx++] = copy;
}

int findRdrNum(char *userId){
    int i;
    for (i = 0; i < str_idx; i++) {
        if (strcmp(userId, userToRdrMap[i]) == 0) {
            return i;
        }
    }
    return -1;
}

void addToBuffer (int rdrNum, char *tuple) {
    int index = buffer[rdrNum].inSlotIndex;
    strcpy(buffer[rdrNum].Tuples[index], tuple);
}


int findScore(struct TopicTotal topictotal[50], char* userId, char* topic1, int size){
  int i;
  int ti = 0;
  int equal = 0;
  int equaluser = 0;
  char *tempuser = userId;
  char *temp = topic1;
  for(i=0; i<size; i++){
    while(*userId != '\0'){
      if(topictotal[i].userID[ti++] == *userId++)
        equaluser = 1;
      else{
        equaluser = 0;
        break;
      }
    }
    ti = 0;
    userId = tempuser;
    if(equaluser == 1){
      while(*topic1 != '\0'){
        if(topictotal[i].topic[ti++] == *topic1++)
          equal = 1;
        else{
          equal = 0;
          break;
        }
      }
      if (equal == 1){
        return topictotal[i].score;
      }
      ti = 0;
      topic1 = temp;
    }
  }
  return -1;
}

int updateScore(struct TopicTotal topictotal[50], char* userId, char* topic1, char *newScore, int size){
  int i;
  int equal = 0;
  int ti = 0;
  int equaluser = 0;
  char *tempuser = userId;
  char* temp = topic1;
  for(i=0; i<size; i++){
    while(*userId != '\0'){
      if(topictotal[i].userID[ti++] == *userId++)
        equaluser = 1;
      else{
        equaluser = 0;
        break;
      }
    }
    ti = 0;
    userId = tempuser;
    if(equaluser == 1){
      while(*topic1 != '\0'){
        if(topictotal[i].topic[ti++] == *topic1++)
          equal = 1;
        else{
          equal = 0;
          break;
        }
      }
      if (equal == 1){
        topictotal[i].score = topictotal[i].score + atoi(newScore);
      }
      topic1 = temp;
      ti = 0;
    }
  }
}


void mapper(){
    char inputTuple[30] = {};
    char buf[30] = {};
    char* oB, *cB, *co, *each, *userId, *line, *eacharray, *outputTuple;
    oB = "(";
    co = ",";
    cB = ")";
    int score, u, pad, i, j, rdrNum;
    size_t len = 0;
    ssize_t read;
    line = NULL;
    u = pad = rdrNum = 0;
    long r;
	  userToRdrMap = malloc(numOfReducers * sizeof(char *));

    while (1) {
        read = getline(&line, &len, stdin);
        if(read == -1){
            *done = 1;          
            break;
        }
        each = strtok(line," ,()");

        while(each != NULL){
            if(u%3 == 0){
                outputTuple = (char *) malloc(1 + strlen(each)+ strlen(oB) );
                strcpy(outputTuple, oB);
                strcat(outputTuple, each);
                userId = each;
            } else if(u%3 == 1){
                char action = *each;
                score = assScore(action);
            } else if(u%3 == 2){
                char * str3 = (char *) malloc(1 + strlen(outputTuple)+ strlen(co) );
                strcpy(str3, outputTuple);
                strcat(str3, co);
                char *str4 = (char *) malloc(1 + strlen(str3)+ strlen(each) );
                strcpy(str4, str3);
                strcat(str4, each);
                for(pad = strlen(each); pad<15; pad++)
                    strcat(str4," ");
                sprintf(buf, "%s,%d)\n", str4, score);
                rdrNum = findRdrNum(userId);
                if(rdrNum > -1){
                } else{
			char *copy = NULL;
			if (str_idx >= numOfReducers) {
				numOfReducers = numOfReducers * 2;
				userToRdrMap = realloc(userToRdrMap, numOfReducers * sizeof(char *));
			}
			copy = malloc(strlen(userId) * sizeof(char));
			copy = strcpy(copy, userId);
			userToRdrMap[str_idx++] = copy;
			rdrNum = findRdrNum(userId);
		}
		
                sem_wait(&buffer[rdrNum].empty_count);
                sem_wait(&buffer[rdrNum].buf_mutex);
                addToBuffer(rdrNum, buf);
                buffer[rdrNum].inSlotIndex = (buffer[rdrNum].inSlotIndex + 1) % (numOfSlots);
                free(outputTuple);
                free(str3);
                free(str4);
                // printf("completed\n");
                sem_post(&buffer[rdrNum].buf_mutex);
                sem_post(&buffer[rdrNum].fill_count);
            }  
            u++;
            each = strtok(NULL," ,()");
        }
        u = 0;
    }
    for(i=0; i<numOfReducers; i++){
        sem_post(&buffer[i].fill_count);
    }


}

void reducer(long tid, Buffer *buffer, int *done){
 //   struct TopicTotal topictotal[50];
    FILE *fp;
    char *each, *userId, *newScore, *topic1;
    char inputTuple[30], outputTuple[50], firstuserId[4];
    int firstTime, equal, ti,j, i, score, index;
    ti = j = i = index = 0;
    equal = firstTime = 1;
    int outindex=0;

    while(1){
        outindex = buffer[tid].outSlotIndex;
        sem_wait(&buffer[tid].fill_count);
        sem_wait(&buffer[tid].buf_mutex);
        if(*done && buffer[tid].inSlotIndex == outindex){
              break;
        }
        strcpy(inputTuple, buffer[tid].Tuples[outindex]);
        char *new = inputTuple;
        buffer[tid].outSlotIndex = (buffer[tid].outSlotIndex + 1) % numOfSlots;
        outindex = buffer[tid].outSlotIndex;
        each = strtok_r(new," ,()", &new);
        userId = each;
        each = strtok_r(new," ,()", &new);
        topic1 = each;
        each = strtok_r(new," ,()", &new);
        newScore = each;
        score = findScore(topictotal, userId, topic1, size);
        if(score == -1){
            strcpy(topictotal[index].userID, userId);
            strcpy(topictotal[index].topic, topic1);
            ti = 0;
            topictotal[index].score = atoi(newScore);
            size = size + 1;
            index = index + 1;
        } else {
            updateScore(topictotal, userId, topic1, newScore, size);
        }
        sem_post(&buffer[tid].buf_mutex);
        sem_post(&buffer[tid].empty_count);
    }
	char filename[256];
	sprintf(filename, "output_%ld.txt", tid);
	FILE *outfile = fopen(filename, "w");
	
	for(j=0; j< index; j++){
		fprintf(outfile, "(%s, %s, %d)\n",  topictotal[j].userID, topictotal[j].topic, topictotal[j].score);
	}
	fclose(outfile);

}


int main (int argc, char *argv[]){

	numOfSlots = atoi(argv[1]);
	numOfReducers = atoi(argv[2]);
	
	int i, j;
  	char *t;
  	
	buffer = (struct Buffer *)mmap(NULL, numOfReducers * sizeof(struct Buffer), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	done = (int *)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*done = 0;

	for(i=0; i<numOfReducers; i++){
		buffer[i].Tuples = mmap(NULL, numOfSlots * sizeof(char *), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		for(j=0; j<numOfSlots; j++){
			t = (char *)mmap(NULL, 30 * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
			t = strcpy(t, "");
			buffer[i].Tuples[j] = t;
		}
		sem_init(&buffer[i].buf_mutex, 1, 1);
		sem_init(&buffer[i].fill_count, 1, 0);
		sem_init(&buffer[i].empty_count, 1, numOfSlots);
	}
	
	if(fork() == 0){
		mapper();
		exit(0);
        }

	for(i=0;i<numOfReducers;i++){ 
		if(fork() == 0){
			reducer(i, buffer, done);
			exit(0);
		}
	}

	wait(NULL);
}
