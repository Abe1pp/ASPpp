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
    char inputTuple[256]; // 假设缓冲区中的字符串长度不超过256
    int index = 0;
    int score, outindex = 0;

    while(1){
        sem_wait(&buffer[tid].fill_count);
        sem_wait(&buffer[tid].buf_mutex);

        // 检查是否完成处理
        if(*done && buffer[tid].inSlotIndex == buffer[tid].outSlotIndex){
            sem_post(&buffer[tid].buf_mutex);
            break;
        }

        strcpy(inputTuple, buffer[tid].Tuples[outindex]);
        buffer[tid].outSlotIndex = (buffer[tid].outSlotIndex + 1) % numOfSlots;
        sem_post(&buffer[tid].buf_mutex);
        sem_post(&buffer[tid].empty_count);

        char userId[20], topic[20];
        // 假设inputTuple中的数据格式为"(userID,score,topic)"
        if(sscanf(inputTuple, "(%[^,],%d,%[^)])", userId, &score, topic) == 3){
            // 成功解析出userId, score和topic
            int found = -1;
            for (int i = 0; i < index; i++) {
                if (strcmp(topictotal[i].userID, userId) == 0 && strcmp(topictotal[i].topic, topic) == 0) {
                    topictotal[i].score += score;
                    found = i;
                    break;
                }
            }
            if (found == -1) { // 没找到，新增一条记录
                strcpy(topictotal[index].userID, userId);
                strcpy(topictotal[index].topic, topic);
                topictotal[index].score = score;
                index++;
            }
        }
    }

    // 输出结果到文件
    char filename[256];
    sprintf(filename, "output_%ld.txt", tid);
    FILE *outfile = fopen(filename, "w");
    if(outfile != NULL){
        for(int j = 0; j < index; j++){
            fprintf(outfile, "(%s, %s, %d)\n", topictotal[j].userID, topictotal[j].topic, topictotal[j].score);
        }
        fclose(outfile);
    } else {
        perror("Failed to open file for writing");
    }
}
