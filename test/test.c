#include <stdio.h>
#include<malloc.h>
#include<time.h>
#include "async.h"


void* showDate(void* env, void* ret) {
	time_t t;
	time(&t);
	printf("现在的时间是： %s\n", ctime(&t));
	return NULL;
}

struct struct_delayed {
	time_t time;
	int delayed;
};
void* delayed_oper(struct struct_delayed* info) {
	time_t t1 = info -> time;
	time_t t2 = time(NULL);
	if (difftime(time(NULL), info -> time) < info -> delayed) {
		return NULL;
	}
	free(info);
	return (void*)1;
}

void delayed(async_env aenv, int s, async_cbfunc cb, void* env) {
	struct struct_delayed* info = (struct struct_delayed*)malloc(sizeof(struct struct_delayed));
	info -> time = time(NULL);
	info -> delayed = s;
	push_operation(aenv, (async_oper)delayed_oper, info, showDate, env, 1);
}

void* async_main(async_env aenv, void* argv) {
	async_cbfunc cb = (async_cbfunc) showDate;
	showDate(NULL, NULL);
	delayed(aenv, 15, cb, NULL); //延时15后输出当前时间
	delayed(aenv, 5, cb, NULL); //延时5后输出当前时间
	return NULL;
}
void* async_handle(void* info) {
	return NULL;
}

int main() {
	int a = 1;
	exec_async(1, 1, async_handle, async_main, &a);
	return 0;
}