#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_REQUESTS 2040
#define MAX_ACC 100


typedef struct {
	int userID;
	int balance;
	pthread_mutex_t lock;
} Account;

typedef struct {
	int from;
	int to;
	int amount;
} Transfer;

typedef struct {
    Transfer requests[MAX_REQUESTS];
    int front, rear;
	int capacity;
	int size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} TransferRequestQueue;

void* bank(void* argv);
void init_request_queue(TransferRequestQueue* queue, int cap);
void enqueue_request(TransferRequestQueue* queue, Transfer request);
Transfer dequeue_request(TransferRequestQueue* queue);
void destroy_request_queue(TransferRequestQueue* queue);
void cleanup();


Account *accounts;
pthread_t *banks;
TransferRequestQueue request_queue;
int accs_num;
int banks_num;
int trfs_num;
char * inputFile;

//thread for transfer operations
void* bank(void *argv){
	int id = *(int *)argv;
	while(1){
		Transfer request = dequeue_request(&request_queue);
		
		if(request.from == request.to){
			pthread_mutex_lock(&accounts[request.to-1].lock);
		}else{
			pthread_mutex_lock(&accounts[request.to-1].lock);
			pthread_mutex_lock(&accounts[request.from-1].lock);
		}	
				
		//process the transfer operation
		accounts[request.from-1].balance = accounts[request.from-1].balance - request.amount;
		printf("\t Bank ID:%d - Transfer %d from %d to %d\n", id, request.amount, request.from, request.to);
		accounts[request.to-1].balance = accounts[request.to-1].balance + request.amount;
		
		if(request.from == request.to){
			pthread_mutex_unlock(&accounts[request.to-1].lock);
		}else{
			pthread_mutex_unlock(&accounts[request.to-1].lock);
			pthread_mutex_unlock(&accounts[request.from-1].lock);
		}
		sleep(1);
	}
}

void init_request_queue(TransferRequestQueue* queue, int cap) {
    queue->front = queue->rear = -1;
	queue->capacity = cap;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void enqueue_request(TransferRequestQueue* queue, Transfer request) {
    pthread_mutex_lock(&queue->mutex);

    // Check if the queue is full
    if ((queue->rear + 1) % queue->capacity == queue->front) {
        fprintf(stderr, "Error: Request queue is full.\n");
        pthread_mutex_unlock(&queue->mutex);
        return;
    }
	queue->size++;
    if (queue->front == -1) {
        queue->front = 0;
    }
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->requests[queue->rear] = request;
    // Signal waiting threads that a new request is available.
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

Transfer dequeue_request(TransferRequestQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    // Wait for a request if the queue is empty.
    while (queue->front == -1) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    Transfer request = queue->requests[queue->front];
    if (queue->front == queue->rear) {
        queue->front = queue->rear = -1;
    } else {
        queue->front = (queue->front + 1) % queue->capacity;
    }

    pthread_mutex_unlock(&queue->mutex);
    return request;
}

void destroy_request_queue(TransferRequestQueue* queue) {
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
}

void cleanup(){
	// cleanup and exit.
	printf("Received signal to shutdown gracefully.\n");
	for (int i = 0; i < accs_num; i++) {
        pthread_mutex_destroy(&accounts[i].lock);
	}
	free(accounts);
	for (int i = 0; i < banks_num; ++i) {
        pthread_cancel(banks[i]);
    }
	destroy_request_queue(&request_queue);

	// create outputfile and write
	printf("All transfers are handled!!!!\n");
	int index = 0;
	for (int i = 0; inputFile[i] != '\0'; i++) {
		if (inputFile[i] >= '0' && inputFile[i] <= '9') {
			index = index * 10 + (inputFile[i] - '0');
		}
    }
	char filename[256];
	sprintf(filename, "output_%d.txt", index);
	FILE *outfile = fopen(filename, "w");
	if (outfile != NULL) {
		for (int i = 0; i < accs_num; i++) {
			fprintf(outfile, "%d %d\n", accounts[i].userID, accounts[i].balance);
		}
		fclose(outfile);
	} else {
		fprintf(stderr, "Error: Error opening file for writing.\n");
	}
	exit(0);
}

int main(int argc, char *argv[]){
	if (argc < 3) {
        printf("Usage: %s <input_file> <number_of_threads>\n", argv[0]);
        return 1;
    }
	int banks_num = atoi(argv[2]);
	inputFile = argv[1];
	char line[500];

	init_request_queue(&request_queue, MAX_REQUESTS);
	// Set up signal handler for graceful shutdown
    signal(SIGINT, cleanup);
	//read all accounts:
	accounts = (Account *)malloc(MAX_ACC * sizeof(Account));
	Transfer* ts = (Transfer*)malloc(MAX_REQUESTS * sizeof(Transfer));
	FILE *file = fopen(argv[1],"r");
	if (file != 0){
		int userID, balance, from, to, amount;
		while(fgets(line, sizeof(line), file)){
			if(sscanf(line,"%d %d",&userID, &balance) ==2){
				accounts[accs_num].userID = userID;
				accounts[accs_num].balance = balance;
				pthread_mutex_init(&accounts[accs_num].lock, NULL);
				accs_num++;
			}
			//read all transf oprations
			else if (sscanf(line, "Transfer %d %d %d\n", &from, &to, &amount)){
				ts[trfs_num].from = from;
				ts[trfs_num].to = to;
				ts[trfs_num].amount = amount;
				trfs_num++;
			}
		}
	}
	fclose(file);
	printf("No. of counter: %d\n", accs_num);
	printf("No. of transfer: %d\n", trfs_num);
	printf("No. of banks: %d\n", banks_num);
	for(int i = 0; i < trfs_num; ++i) {
		enqueue_request(&request_queue, ts[i]);
	}

	banks = (pthread_t *)malloc(banks_num * sizeof(pthread_t));
	int bankIDs[banks_num];
	for (int i = 0; i < banks_num; i++) {
		bankIDs[i] = i;
		if (pthread_create(&banks[i], NULL, bank, &bankIDs[i]) != 0) { // 将新分配的 Transfer 结构传递给线程
			fprintf(stderr, "Error: Failed to create thread %d.\n", i);
		} else{
			printf("Create bank thread, No. of %d\n", i);
		}
    }
	
	for (int i = 0; i < banks_num; ++i) {
        pthread_join(banks[i], NULL);
    }

	return 0;

}
	
	

	
	
