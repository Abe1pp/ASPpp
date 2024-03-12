#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>

typedef struct Tuple{
	char userID[5] ;   
	char topic[16]; 
	int score;
	} Tuple;
	
typedef struct Buffer {
	Tuple *buffer;
	int bufferSize;
	int count; //amount of tuples in buffer
	int in; //next insert position
	int out; //next take out position
	bool done;
	pthread_mutex_t mutex;
	pthread_cond_t empty;
	pthread_cond_t full;
	}Buffer;

Buffer *buffers;

typedef struct ScoreNode {
    char userID[5];
    char topic[16];
    int score;
    struct ScoreNode *next;
} ScoreNode;

ScoreNode *scoreList = NULL; 

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

//hash for mapper
unsigned int Hash(char* userID, int num_reducer) { 
    	unsigned int hash = 0;
    	int c;

    	while ((c = *userID++)) {
        	hash = c + (hash << 6) + (hash << 16) - hash;
    	}

	//unsigned int hash = atoi(userID);
	//unsigned int hash = (userID[0] - '0') * 1000 + (userID[1] - '0') * 100 + (userID[2] - '0') * 10 + (userID[3] - '0');
    	return hash % num_reducer;
}


// find or create a new ScoreNode for reducer
ScoreNode* findOrCreateScoreNode(ScoreNode **scoreList, char *userID, char *topic) {
    ScoreNode *current = *scoreList;
    while (current != NULL) {
        if (strcmp(current->userID, userID) == 0 && strcmp(current->topic, topic) == 0) {
            return current;
        }
        current = current->next;
    }

    //create new if there is no one
    ScoreNode *newNode = (ScoreNode *)malloc(sizeof(ScoreNode));
    strcpy(newNode->userID, userID);
    strcpy(newNode->topic, topic);
    newNode->score = 0;  //init
    newNode->next = *scoreList;  //insert to the head of the list
    *scoreList = newNode;
    return newNode;
}


//cleanup score list for reducer
void cleanupScoreList(ScoreNode **scoreList) {
    ScoreNode *current = *scoreList;
    while (current != NULL) {
        ScoreNode *temp = current;
        current = current->next;
        free(temp);
    }
    *scoreList = NULL;
}


void *mapperThread(void *param);
void *reducerThread(void *param);

int num_slot;
int num_reducer;


int main(int argc, char *argv[]){

	num_slot = atoi(argv[1]);
	num_reducer = atoi(argv[2]);
	
	pthread_t mapper;
	pthread_t reducer[num_reducer];
		
	//initialize buffers
	buffers = (Buffer *)malloc(num_reducer * sizeof(Buffer));
	for (int i = 0; i < num_reducer; i++){
		buffers[i].bufferSize = num_slot;
		buffers[i].buffer = (Tuple *)malloc(num_slot * sizeof(Tuple));
		buffers[i].count = 0;
		buffers[i].in = 0;
		buffers[i].out = 0;
		buffers[i].done = false;
		pthread_mutex_init(&buffers[i].mutex, NULL);
		pthread_cond_init(&buffers[i].full, NULL);
		pthread_cond_init(&buffers[i].empty, NULL);
		}
	
	//create threads
	pthread_create(&mapper, NULL, mapperThread, NULL);
	
	for (int i = 0; i < num_reducer; i++){
		pthread_create(&reducer[i], NULL, reducerThread, (void *)(intptr_t)i);
	} 
		
	//join threads
	pthread_join(mapper, NULL);
	for (int i = 0; i < num_reducer; i++){
		pthread_join(reducer[i], NULL);
		}
		
	//cleanup and exit
	for (int i = 0; i < num_reducer; i++){
		pthread_mutex_destroy(&buffers[i].mutex);
		pthread_cond_destroy(&buffers[i].full);
		pthread_cond_destroy(&buffers[i].empty);
		}
		
	return 0;

}

void *mapperThread(void *arg){
	char action;    
	char bf[5020];	
	Tuple tuple;

	while (fscanf(stdin, "(%4[^,], %c, %15[^)])", tuple.userID, &action, tuple.topic) ==3 ){ //process the first line of every input file
		tuple.score = assScore(action);
		//strcpy(tuple.userID, userID);
		//strcpy(tuple.topic, topic);
		
		int reduceridx = Hash(tuple.userID, num_reducer);
		printf("Mapper: assigning tuple(%s, %s, %d) to reducer %d\n", tuple.userID, tuple.topic, tuple.score, reduceridx);
		Buffer *buf = &buffers[reduceridx];
            	pthread_mutex_lock(&buf->mutex);
            	while (buf->count == buf->bufferSize) {
                	pthread_cond_wait(&buf->empty, &buf->mutex);  
            	}	

                // input tuple to buffer
            	buf->buffer[buf->in] = tuple;
            	buf->in = (buf->in + 1) % buf->bufferSize;
            	buf->count++;

            	pthread_cond_signal(&buf->full);  // impart the waitting reducer
            	pthread_mutex_unlock(&buf->mutex);
	}
	
	while (fgets(bf, sizeof(bf),stdin) != NULL){// procss the remaining lines.
		if (sscanf(bf, "(%4[^,], %c, %15[^)])", tuple.userID, &action, tuple.topic) ==3 ){
			tuple.score = assScore(action);
			int reduceridx = Hash(tuple.userID, num_reducer);
			
			Buffer *buf = &buffers[reduceridx];
            		pthread_mutex_lock(&buf->mutex);
            		while (buf->count == buf->bufferSize) {
                		pthread_cond_wait(&buf->empty, &buf->mutex);
            		}	

            		// input tuple to buffer
            		buf->buffer[buf->in] = tuple;
            		buf->in = (buf->in + 1) % buf->bufferSize;
            		buf->count++;

            		pthread_cond_signal(&buf->full);  // impart the waitting reducer
            		pthread_mutex_unlock(&buf->mutex);

		}
	}				
	// impart all reducers that the end of input file
    	for (int i = 0; i < num_reducer; i++) {
       	pthread_mutex_lock(&buffers[i].mutex);
        	buffers[i].done = true;
        	pthread_cond_broadcast(&buffers[i].full);  
       	pthread_mutex_unlock(&buffers[i].mutex);
   	 }

  	 return NULL;


}

void *reducerThread(void *arg) {
	int reduceridx = (intptr_t)arg;
	Buffer *buf = &buffers[reduceridx];
	printf("reducer %d start\n", reduceridx);
    
    //create local scoreList
	ScoreNode *scoreList = NULL;
    
    //the loop to produce the tuples
	while (1) {
		pthread_mutex_lock(&buf->mutex);
		while (buf->count == 0 && !buf->done) {
			pthread_cond_wait(&buf->full, &buf->mutex);
		}
	if (buf->count == 0 && buf->done) {
		pthread_mutex_unlock(&buf->mutex);
		break;
	}

	Tuple tuple = buf->buffer[buf->out];
	buf->out = (buf->out + 1) % buf->bufferSize;
	buf->count--;
	pthread_cond_signal(&buf->empty);
	pthread_mutex_unlock(&buf->mutex);

	printf("reducer %d: doing tuple(%s, %s, %d)\n", reduceridx, tuple.userID, tuple.topic, tuple.score);

	ScoreNode *node = findOrCreateScoreNode(&scoreList, tuple.userID, tuple.topic);
	node->score += tuple.score;
        
	}
    
    
	//create output file
	char filename[256];
	sprintf(filename, "output_%d.txt", reduceridx);
	FILE *outfile = fopen(filename, "w");
	if (!outfile) {
		perror("Failed to open output file");
		cleanupScoreList(&scoreList);
		return NULL;
	}
	for (ScoreNode *node = scoreList; node != NULL; node = node->next) {
		fprintf(outfile, "(%s, %s, %d)\n", node->userID, node->topic, node->score);
	}
	fclose(outfile);

	cleanupScoreList(&scoreList);

	return NULL;
}

