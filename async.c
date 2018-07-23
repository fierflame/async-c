#include<malloc.h>
#include <unistd.h>
#define ERROR_MALLOC 1

typedef void*(*async_cb_func)(void* context, void* res, char is_destroy);
typedef void*(*async_oper_func)(void*, char is_destroy);
typedef void*(*async_handle_func)(void*);

//链表节点
typedef struct struct_link_node{
	struct struct_link_node* next;
	void* value;
}* link_node;
//异步操作
typedef struct struct_async_operation{
	async_oper_func oper;
	void* argv;
	async_cb_func callback;
	void* env;
	int level;
}* async_operation;
//回调信息
typedef struct struct_callback_info{
	async_cb_func callback;
	void * env;
	void * result;
}* callback_info;
//异步执行环境
typedef struct struct_async_env{
	int level; //开启的等级
	link_node* cbink; //回调链起点数组
	int cbink_length;//剩余回调函数数量
	int max_time; //单循环最大处理次数
	link_node oper_link[2];
	int oper_link_length;//剩余回调函数数量
}* async_env;

int push_operation(async_env, async_oper_func, void * argv, async_cb_func, void * env, int level);
int exec_operation(async_env);
async_env async_init(int level, int max_time);
void* async_destroy(async_env);
int push_callback(async_env, async_cb_func, void* env, void* result, int level);
void* async_exec(async_env, async_handle_func);

/**
 * 创建链表节点
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

/**
 * 释放链表节点
 * @param  ln 连接节点
 * @return    节点的内容
 */
void * free_link_node(link_node ln) {
	void * value = ln -> value;
	free(ln);
	return value;
}

/**
 * 添加到链表尾部
 * @param  link_info 链接信息
 * @param  value     要添加的内容
 * @return           是否添加失败，如果添加成功则为0，否则为1
 */
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

/**
 * 操作入栈
 * @param aenv  异步执行环境
 * @param oper  操作处理函数
 * @param argv  操作处理函数的参数
 * @param callback  回调函数
 * @param env   回调函数环境
 * @param level 回调函数级别
 * @return		错误信息，如果返回0，表示无错误
 */
int push_operation(
	async_env aenv,
	async_oper_func oper,
	void * argv,
	async_cb_func callback,
	void * env,
	int level
) {
	async_operation ao;
	int ret;

	ao = (async_operation)malloc(sizeof(struct struct_async_operation));
	if (!ao) {
		return ERROR_MALLOC;
	}
	ao -> oper = oper;
	ao -> argv = argv;
	ao -> callback = callback;
	ao -> env = env;
	ao -> level = level;
	ret = append_to_link(aenv -> oper_link, ao);
	aenv -> oper_link_length++;
	return ret;

}

/**
 * 执行操作处理
 * @param aenv  异步执行环境
 * @return		实际结束的异步操作数
 */
int exec_operation(async_env aenv) {
	link_node* pl = aenv -> oper_link;
	async_operation ao;
	void* ret = NULL;
	int execed = 0;

	while(*pl != NULL) {
		//获取真正的异步操作参数
		ao = (async_operation)(*pl)->value;
		//执行异步操作，并获取异步操作结果
		ret = ao -> oper(ao -> argv, 0);
		if (ret == NULL) {
			//返回值为 NULL表示没有异步操作没有结束
			//直接处理下一项
			pl = &((*pl)->next);
			continue;
		}
		//有返回值表示异步操作结束
		//将异步回调函数压入回调链表
		*pl = (*pl) -> next;
		aenv -> oper_link_length--;
		execed++;
		push_callback(aenv, ao -> callback, ao -> env, ret, ao -> level);
		free(ao);
	}

	if (aenv -> oper_link[0] == NULL) {
		aenv -> oper_link[1] = NULL;
	}
	return execed;
}


/**
 * 初始化并获得异步操作环境
 * @param  level    异步操作等级等级数
 * @param  max_time 单次轮训最大执行的回调数
 * @return          异步执行环境，如果返回NULL，表示初始化失败
 */
async_env async_init(int level, int max_time) {
	async_env aenv;
	link_node* cbink;

	aenv = (async_env)malloc(sizeof(struct struct_async_env));
	if (!aenv) {
		//内存申请失败，直接返回空指针
		return NULL;
	}
	//级别修整
	if (level < 1) {
		level = 1;
	}
	aenv -> level = level;
	//申请回调函数指针内存
	cbink = aenv -> cbink = (link_node*)malloc(sizeof(void *) * level * 2);
	if (!cbink) {
		free(aenv);
		return NULL;
	}
	//初始化请回调函数指针内存
	for (int i = level * 2; i >= 0; i--) {
		cbink[i] = NULL;
	}
	if (max_time <= 0) {
		max_time = -1;
	}
	aenv -> max_time = max_time;
	aenv -> oper_link[0] = aenv -> oper_link[1] = NULL;
	aenv -> cbink_length = aenv -> oper_link_length = 0;
	return aenv;
}

void* async_destroy(async_env aenv) {
	int i;
	int level;
	link_node* cbink;
	link_node ln;
	callback_info info;

	async_operation ao;
	link_node* pl;

	level = aenv -> level;
	cbink = aenv -> cbink;

	for(i = 0; i < level; i++) {
		while(cbink[i * 2]) {
			//获取函数
			ln = cbink[i * 2];
			cbink[i * 2] = ln -> next;
			info = (callback_info)free_link_node(ln);
			//回收 info -> env, info -> result
			info -> callback(info -> env, info -> result, 1);
		}
	}
	free(aenv -> cbink);

	pl = aenv -> oper_link;
	while(*pl != NULL) {
		//获取真正的异步操作参数
		ao = (async_operation)(*pl)->value;
		//回收 ao -> argv
		ao -> oper(ao -> argv, 1);
		//回收 ao -> env
		ao -> callback(ao -> env, NULL, 1);
		free(ao);
		*pl = (*pl) -> next;
	}

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
int push_callback(
	async_env aenv,
	async_cb_func callback,
	void* env,
	void* result,
	int level
) {
	callback_info info;
	int ret;

	//创建回调信息
	info = (callback_info)malloc(sizeof(struct struct_callback_info));
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
	ret = append_to_link(aenv -> cbink + level * 2, info);
	aenv -> cbink_length++;
	return ret;
}


/**
 * 执行回调函数
 * @param  aenv      环境
 * @param  handle 回调函数指针
 * @param  result   结果
 * @param  env      环境
 * @param  level    级别
 * @return          错误信息
 */
void* exec_cb(
	async_env aenv,
	async_handle_func handle,
	link_node* cbink,
	int time,
	int level
) {
	void* ret;
	link_node ln;
	callback_info info;

	for(int i = 0; i < level; i++) {
		while(cbink[i * 2]) {
			//获取函数
			ln = cbink[i * 2];
			cbink[i * 2] = ln -> next;
			info = (callback_info)free_link_node(ln);
			aenv -> cbink_length--;
			//执行函数
			ret = info -> callback(info -> env, info -> result, 0);
			if (ret && (handle == NULL || (ret = handle(ret)) != NULL)) {
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

/**
 * 异步执行
 */
void* async_exec(
	async_env aenv,
	async_handle_func handle
) {
	int level;
	int max_time;
	link_node* cbink;
	void* ret;

	level = aenv -> level;
	max_time = aenv -> max_time;
	cbink = aenv -> cbink;
	while(aenv -> cbink_length | aenv -> oper_link_length) {
		if (exec_operation(aenv) == 0 && aenv -> cbink_length == 0) {
			//让出CPU一周期
			sleep(0);
		} else {
			//处理函数
			ret = exec_cb(aenv, handle, cbink, max_time, level);
			if (ret != NULL) {
				return ret;
			}
		}
	}
	return NULL;
}

/**
 * 执行异步操作
 * @param	level
 * @param	max_time
 * @param	handle
 * @param	main
 * @param	main_argv
 */
void* exec_async(
	int level,
	int max_time,
	async_handle_func handle,
	void* (*main)(async_env, void*),
	void* main_argv
) {
	async_env aenv;
	void * ret;
	//初始化环境
	aenv = async_init(level, max_time);
	//执行主函数
	if (main != NULL) {
		ret = main(aenv, main_argv);
		if (ret) {
			if (handle == NULL || (ret = handle(ret)) != NULL) {
				async_destroy(aenv);
				return ret;
			}
		}
	}
	ret = async_exec(aenv, handle);
	async_destroy(aenv);
	return ret;
}
