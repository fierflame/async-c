typedef void* async_env;
typedef void*(*async_cbfunc)(void*env, void*res);
typedef void*(*async_oper)(void*);
int push_operation(async_env aenv, async_oper, void * argv, async_cbfunc callback, void* env, int level);
async_env async_init(int level, int max_time);
void* async_destroy(async_env aenv);
int push_callback(async_env aenv, async_cbfunc callback, void* env, void* result, int level);
void* async_exec(async_env aenv, void*(*handle)(void*));
void* exec_async(int level, int max_time, void*(*handle)(void*), void* (*main)(async_env,void*), void*);