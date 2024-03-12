# ASPpp

void mapper() {
    Tuple tuple; // 假设已定义Tuple结构体
    char action;
    int score, rdrNum;

    // 初始化工作，例如为userToRdrMap分配初始内存
    userToRdrMap = malloc(numOfReducers * sizeof(char *));
    memset(userToRdrMap, 0, numOfReducers * sizeof(char *)); // 初始化指针数组为NULL
    str_idx = 0; // 用户到reducer映射的当前索引

    while (fscanf(stdin, "(%4[^,], %c, %15[^)])", tuple.userID, &action, tuple.topic) == 3) {
        // 解析成功后处理数据
        tuple.score = assScore(action); // 根据操作计算分数

        // 查找或新增用户到reducer的映射
        rdrNum = findRdrNum(tuple.userID);
        if (rdrNum == -1) { // 如果没有找到映射，添加新的映射
            addToUser(tuple.userID); // 添加新的用户ID到映射中
            rdrNum = findRdrNum(tuple.userID); // 再次查找刚刚添加的映射
        }

        // 确保rdrNum有效
        if (rdrNum >= 0) {
            // 向对应reducer的缓冲区添加元组
            sem_wait(&buffer[rdrNum].empty_count);
            sem_wait(&buffer[rdrNum].buf_mutex);
            
            // 假设我们有函数addToBuffer，该函数接受reducer编号和元组
            addToBuffer(rdrNum, tuple); // 需要根据实际情况实现该函数

            sem_post(&buffer[rdrNum].buf_mutex);
            sem_post(&buffer[rdrNum].fill_count);
        }
    }

    // 标记完成，向所有reducer发送结束信号
    for(int i = 0; i < numOfReducers; i++) {
        sem_post(&buffer[i].fill_count); // 假设每个reducer都有机会检查并结束
    }
}
