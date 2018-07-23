typedef void* async_env;
typedef void*(*async_cb_func)(void* context, void* res, char is_destroy);
typedef void*(*async_oper_func)(void*, char is_destroy);
typedef void*(*async_handle_func)(void*);
typedef void*(*async_main_func)(async_env, void*);

int push_operation(async_env, async_oper_func, void* argv, async_cb_func, void* context, int level);
async_env async_init(int level, int max_time);
void* async_destroy(async_env);
int push_callback(async_env, async_cb_func, void* context, void* result, int level);
void* async_exec(async_env, async_handle_func);
void* exec_async(int level, int max_time, async_handle_func, async_main_func, void* main_argv);