#include <stdio.h>
#include<malloc.h>
#include<time.h>
#include "../async.h"


void* showDate(void* context, void* ret, char is_destroy) {
	if (is_destroy) {
		return NULL;
	}
	time_t t;
	time(&t);
	printf("现在的时间是： %s\n", ctime(&t));
	return NULL;
}

/**
 * 异步延时处理函数
 * @param data       相关数据，依次保存
 * @param is_destroy [description]
 */
void* delayed_oper(void* endData, char is_destroy) {
	if (is_destroy) {
		free(endData);
		return NULL;
	}
	if (difftime(time(NULL), *(time_t*)endData) < 0) {
		return NULL;
	}
	free(endData);
	return (void*)1;
}
void delayed(async_env aenv, int s, async_cb_func cb, void* context) {
	void* endData = malloc(sizeof(time_t));
	*(time_t*)endData = time(NULL) + s;
	push_operation(aenv, delayed_oper, endData, cb, context, 1);
}

void* async_main(async_env aenv, void* argv) {
	delayed(aenv, 15, showDate, NULL); //延时15后输出当前时间
	showDate(NULL, NULL, 0);//立刻显示当前时间
	delayed(aenv, 5, showDate, NULL); //延时5后输出当前时间
	return NULL;
}
void* handle(void* info) {
	return NULL;
}

int main() {
	int a = 1;
	exec_async(1, -1, handle, async_main, &a);
	return 0;
}