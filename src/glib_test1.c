#include <stdio.h>
#include <glib.h>

/* timeout functions */
/* typedef unsigend int boolean */
/* typedef void* gpointer */
gboolean count_down(gpointer data);
gboolean cancel_fire(gpointer data);

/* idle functions */
gboolean say_idle(gpointer data);

int main(int argc, char const *argv[]) {
    /* 1.创建一个 GMainLoop 结构体对象，作为一个主事件循环 */
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    /* 2.添加超时事件源 */
    g_timeout_add(1000, count_down, NULL);
    g_timeout_add(8000, cancel_fire, loop);

    /* 3.添加空闲函数，没有更高优先级事件时，空闲函数就可以被执行 */
    g_idle_add(say_idle, NULL);

    /* 4.循环检查事件源中是否有新事件进行分发，当某事件处理函数调用 g_main_loop_quit()，函数退出 */
    g_main_loop_run(loop);

    /* 5.减少loop引用计数，如果计数为0，则会释放loop资源 */
    g_main_loop_unref(loop);

    return 0;
}

gboolean count_down(gpointer data) {
    static int count = 10;

    if (count < 1) {
        printf(">>> count_down() return FALSE\n");
        return FALSE;
    }

    printf(">>> count_down() %4d\n", count--);
    return TRUE;
}

gboolean cancel_fire(gpointer data) {
    GMainLoop *loop = data;
    printf(">>> cancel_fire() quit \n");
    g_main_loop_quit(loop);

    return FALSE;
}

/* 
 * FALSE，该事件源将被删除
 * TRUE，该事件源会在没有更高优先级事件时，再次运行
 */
gboolean say_idle(gpointer data) {
    printf(">>> say_idle() idle \n");
    return FALSE;
}