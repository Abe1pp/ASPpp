# ASPpp

void mapper() {
    Tuple tuple; // 假设已定义Tuple结构体
    char action;
    char bufferEntry[50]; // 假设足以存储转换后的字符串

    while (fscanf(stdin, "(%4[^,], %c, %15[^)])", tuple.userID, &action, tuple.topic) == 3) {
        tuple.score = assScore(action); // 根据操作计算分数

        // 将Tuple转换为字符串
        sprintf(bufferEntry, "(%s, %s, %d)", tuple.userID, tuple.topic, tuple.score);

        // 查找或新增用户到reducer的映射
        int rdrNum = findRdrNum(tuple.userID);
        if (rdrNum == -1) { // 如果没有找到映射，添加新的映射
            addToUser(tuple.userID); // 添加新的用户ID到映射中
            rdrNum = findRdrNum(tuple.userID); // 再次查找刚刚添加的映射
        }

        // 向对应reducer的缓冲区添加元组字符串
        if (rdrNum >= 0) {
            int index = buffer[rdrNum].inSlotIndex;
            sem_wait(&buffer[rdrNum].empty_count);
            sem_wait(&buffer[rdrNum].buf_mutex);

            strcpy(buffer[rdrNum].Tuples[index], bufferEntry);
            buffer[rdrNum].inSlotIndex = (buffer[rdrNum].inSlotIndex + 1) % numOfSlots;

            sem_post(&buffer[rdrNum].buf_mutex);
            sem_post(&buffer[rdrNum].fill_count);
        }
    }

    // 标记完成，向所有reducer发送结束信号
    for(int i = 0; i < numOfReducers; i++) {
        sem_post(&buffer[i].fill_count); // 假设每个reducer都有机会检查并结束
    }
}


void reducer(long tid, Buffer *buffer, int *done){
    // 假设已经定义了struct TopicTotal topictotal[50];
    FILE *fp;
    char *userId, *topic, *scoreStr;
    char inputTuple[256]; // 增加了容量以容纳更长的输入
    int score, j, index = 0;
    int outindex = 0;

    while(1){
        sem_wait(&buffer[tid].fill_count);
        sem_wait(&buffer[tid].buf_mutex);

        // 检查是否处理完成
        if(*done && buffer[tid].inSlotIndex == buffer[tid].outSlotIndex){
            sem_post(&buffer[tid].buf_mutex);
            break;
        }

        strcpy(inputTuple, buffer[tid].Tuples[buffer[tid].outSlotIndex]);
        buffer[tid].outSlotIndex = (buffer[tid].outSlotIndex + 1) % numOfSlots;

        // 解析从mapper获取的字符串
        if(sscanf(inputTuple, "(%[^,],%[^,],%d)", userId, topic, &score) == 3){
            // 在这里，我们假设userId, topic 是足够大的缓冲区
            // 处理获取的数据
            int scoreIndex = findScore(topictotal, userId, topic, size);
            if(scoreIndex == -1){
                // 添加新记录
                strcpy(topictotal[index].userID, userId);
                strcpy(topictotal[index].topic, topic);
                topictotal[index].score = score;
                index++;
                size++;
            } else {
                // 更新现有记录的分数
                topictotal[scoreIndex].score += score;
            }
        }

        sem_post(&buffer[tid].buf_mutex);
        sem_post(&buffer[tid].empty_count);
    }

    // 输出结果到文件
    char filename[256];
    sprintf(filename, "output_%ld.txt", tid);
    FILE *outfile = fopen(filename, "w");
    for(j = 0; j < index; j++){
        fprintf(outfile, "(%s, %s, %d)\n", topictotal[j].userID, topictotal[j].topic, topictotal[j].score);
    }
    fclose(outfile);
}

co.c: In function ‘reducer’:
co.c:336:34: warning: passing argument 3 of ‘fgets’ from incompatible pointer type [-Wincompatible-pointer-types]
  336 |     while (fgets(bf, sizeof(bf), inputTuple) != NULL){
      |                                  ^~~~~~~~~~
      |                                  |
      |                                  char *
In file included from co.c:1:
/usr/include/stdio.h:564:14: note: expected ‘FILE * restrict’ {aka ‘struct _IO_FILE * restrict’} but argument is of type ‘char *’
  564 | extern char *fgets (char *__restrict __s, int __n, FILE *__restrict __stream)
      |              ^~~~~
