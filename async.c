#include<malloc.h>
#define ERROR_MALLOC 1
#include <stdio.h>

typedef struct struct_link_node{
	struct struct_link_node* next;
	void* value;
}* link_node;
//异步操作
typedef struct struct_async_operation{
	void* (*oper)(void* argv);
	void* argv;
	void* (*callback)(void* env, void* result);
	void* env;
	int level;
}* async_operation;
typedef struct struct_callback_info{
	void* (*callback)(void* env, void* result);
	void * env;
	void * result;
}* callback_info;
typedef struct struct_async_env{
	int level; //开启的等级
	link_node* cbink; //回调链起点数组
	int cbink_length;//剩余回调函数数量
	int max_time; //单循环最大处理次数
	link_node oper_link[2];
	int oper_link_length;//剩余回调函数数量
}* async_env;

int push_operation(async_env aenv, void* (*oper)(void*), void * argv, void* (*callback)(void*, void*), void * env, int level);
int exec_operation(async_env aenv);
async_env async_init(int level, int max_time);
void* async_destroy(async_env aenv);
int push_callback(async_env aenv, void* (*callback)(void*, void*), void* env, void* result, int level);
void* async_exec(async_env aenv, void*(*handle)(void*));





/**
 * 创建链结点
 * @param  value    结点内容
 * @param  previous 上一个结点
 * @return          结点指针
 */
link_node create_link_node(void * value, link_node previous) {
	link_node ln = (link_node)malloc(sizeof(struct struct_link_node));
	if (ln == NULL) {
		return ln;
	}
	ln -> value = value;
	if (previous != NULL) {
		ln -> next = previous -> next;
		previous -> next = ln;
	} else {
		ln -> next = NULL;
	}
	return ln;
}

void * free_link_node(link_node ln) {
	void * value = ln -> value;
	free(ln);
	return value;
}

int append_to_link(link_node* link_info, void* value) {
	link_node ln = create_link_node(value, link_info[0] == NULL ? NULL : link_info[1]);
	if (!ln) {
		free(value);
		return 1;
	}
	if (link_info[0] == NULL) {
		link_info[0] = link_info[1] = ln;
	} else {
		link_info[1] = ln;
	}
	return 0;
}


//操作链
//对于默认的，分别表示首项和尾项
//对中间的链路，分别表示首项和操作参数

/**
 * 操作入栈
 * @param oper  操作处理函数
 * @param argv  操作处理函数的参数
 * @param callback  回调函数
 * @param env   回调函数环境
 * @param level 回调函数级别
 */
int push_operation(async_env aenv, void* (*oper)(void*), void * argv, void* (*callback)(void*, void*), void * env, int level) {
	async_operation ao = (async_operation)malloc(sizeof(struct struct_async_operation));
	if (!ao) {
		return ERROR_MALLOC;
	}
	ao -> oper = oper;
	ao -> argv = argv;
	ao -> callback = callback;
	ao -> env = env;
	ao -> level = level;
	int ret = append_to_link(aenv -> oper_link, ao);
	aenv -> oper_link_length++;
	return ret;

}
/**
 * 执行操作处理
 */
int exec_operation(async_env aenv) {
	link_node* pl = aenv -> oper_link;
	async_operation ao;
	void* ret = NULL;
	int execed = 0;

	while(*pl != NULL) {
		//获取真正的异步操作参数
		ao = (async_operation)(*pl)->value;
		ret = ao -> oper(ao -> argv);
		if (ret) {
			*pl = (*pl) -> next;
			aenv -> oper_link_length--;
			execed++;
			push_callback(aenv, ao -> callback, ao -> env, ret, ao -> level);
			free(ao);
		} else {
			pl = &((*pl)->next);
		}
	}
	if (aenv -> oper_link[0] == NULL) {
		aenv -> oper_link[1] = NULL;
	}
	return execed;
}



async_env async_init(int level, int max_time) {
	async_env aenv = (async_env)malloc(sizeof(struct struct_async_env));
	if (!aenv) {
		return NULL;
	}
	//级别修整
	if (level < 1) {
		level = 1;
	}
	aenv -> level = level;
	//申请回调函数指针内存
	link_node* cbink = aenv -> cbink = (link_node*)malloc(sizeof(void *) * level * 2); //<!!!>
	if (!aenv -> cbink) {
		free(aenv);
		return NULL;
	}
	//初始化请回调函数指针内存
	for (int i = level * 2; i >= 0; i--) {
		cbink[i] = NULL;
	}
	if (max_time < 1) {
		max_time = 1;
	}
	aenv -> max_time = max_time;
	aenv -> oper_link[0] = aenv -> oper_link[1] = NULL;
	aenv -> cbink_length = aenv -> oper_link_length = 0;
	return aenv;
}

void* async_destroy(async_env aenv) {
	//TODO: 回收aenv -> link_node
	free(aenv);
}

/**
 * 将回调压入栈
 * @param  callback 回调函数指针
 * @param  result   结果
 * @param  env      环境
 * @param  level    级别
 * @return          错误信息
 */
int push_callback(async_env aenv, void* (*callback)(void*, void*), void* env, void* result, int level) {
	//创建回调信息
	callback_info info = (callback_info)malloc(sizeof(struct struct_callback_info));
	if (info == NULL) {
		return ERROR_MALLOC;
	}
	info -> callback = callback;
	info -> env = env;
	info -> result = result;
	//等级处理
	if (level < 0) {
		level = 0;
	} else if (level >= aenv -> level) {
		level = aenv -> level - 1;
	}
	int ret = append_to_link(aenv -> cbink + level * 2, info);
	aenv -> cbink_length++;
	return ret;
}


void* exec_cb(async_env aenv, void*(*handle)(void*), link_node* cbink, int time, int level) {
	for(int i = 0; i < level; i++) {
		while(cbink[i * 2]) {
			//获取函数
			link_node ln = cbink[i * 2];
			cbink[i * 2] = ln -> next;
			callback_info info = (callback_info)free_link_node(ln);
			aenv -> cbink_length--;
			//执行函数
			void* ret = info -> callback(info -> env, info -> result);
			if (handle == NULL || (ret = handle(ret)) != NULL) {
				return ret;
			}
			time--;
			if (!time) {
				break;
			}
		}
		if (!time) {
			break;
		}
	}
	return NULL;
}


void* async_exec(async_env aenv, void*(*handle)(void*)) {
	int level = aenv -> level;
	int max_time = aenv -> max_time;
	link_node* cbink = aenv -> cbink;
	while(aenv -> cbink_length | aenv -> oper_link_length) {
		if (exec_operation(aenv) == 0 && aenv -> cbink_length == 0) {
			//TODO: 让出CPU一周期
		} else {
			//处理函数
			void* ret = exec_cb(aenv, handle, cbink, max_time, level);
			if (ret != NULL) {
				return ret;
			}
		}
	}
	return NULL;
}

void* exec_async(int level, int max_time, void*(*handle)(void*), void* (*main)(async_env, void*), void* main_argv) {
	async_env aenv = async_init(level, max_time);
	void * ret;
	if (main != NULL) {
		ret = main(aenv, main_argv);
		if (handle == NULL || (ret = handle(ret)) != NULL) {
			return ret;
		}
	}
	ret = async_exec(aenv, handle);
	async_destroy(aenv);
	return ret;
}
