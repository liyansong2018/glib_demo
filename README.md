博客：[Glib 主事件循环轻度分析与编程应用](https://blog.csdn.net/song_lee/article/details/116809089)

# 1 glib 事件循环概述

`glib` 是一个跨平台、用 C 语言编写的若干底层库的集合。编写案例最好能够结合 [glib 源码](https://github.com/GNOME/glib)，方便随时查看相关函数定义。`glib` 实现了完整的事件循环分发机制。有一个主循环负责处理各种事件。事件通过事件源描述，常见的**事件源**

- 文件描述符（文件、管道和 socket）
- 超时
- idle 事件

当然，也可以自定义事件源，通过 `glib` 提供的函数 `g_source_attach()` 就可以手动添加我们自定义的新类型事件源。可参考 [2.2](##2.2 案例2 glib 自定义事件源)

事件源默认会被分配一个优先级，默认为 `G_PRIORITY_DEFAULT`(200)，数字越小，优先级越高。当然也可以在添加空闲函数（`g_idle_add_full`）的时候，指定该事件源的优先级。具体应用还是结合后面的示例以及源码动手实践。

## 1.1 glib 主事件循环涉及的重要数据结构
**GMainContext**

为了让多组独立事件源能够在不同的线程中被处理，每个事件源都会关联一个`GMainContext`。一个线程只能运行一个`GMainContext`，但是在其他线程中能够对事件源进行添加和删除操作。`GMainContext` 定义如下
```c
/**
 * GMainContext:
 *
 * The `GMainContext` struct is an opaque data
 * type representing a set of sources to be handled in a main loop.
 */
typedef struct _GMainContext            GMainContext;
```
**GMainLoop**

`GMainLoop` 数据类型代表了一个主事件循环。通过`g_main_loop_new()`来创建`GMainLoop`对象。在添加完初始事件源后执行`g_main_loop_run()`，主循环将持续不断的检查每个事件源产生的新事件，然后分发它们，直到处理来自某个事件源的事件的时候触发了`g_main_loop_quit()`调用退出主循环为止。
```c
struct _GMainLoop
{
  GMainContext *context;
  gboolean is_running; /* (atomic) */
  gint ref_count;  /* (atomic) */
};
```

*其实很多博客只是介绍了上述内容，如果你对 `glib` 本身就不熟悉，还是很难明白其中的道理。因此一定要结合源码，以及动手调试，才能够更好的理解 `glib` 事件循环。所以，动手试试笔者第二节改写的三个案例吧。*
## 1.2 glib / glibc /libc 傻傻分不清楚
- `glibc` 和 `libc` 都是` Linux` 下的 C 函数库
- **libc** 是 `Linux` 下的 **ANSI C** 函数库
- **glibc** 是 `Linux` 下的 **GUN C** 函数库
- **glib** 是 **GTK+** 的基础库，一个综合用途的实用的轻量级的C程序库，与 glibc 没有实际联系

> ANSI C 函数库是基本的 C 语言函数库，包含了 C 语言最基本的库函数。GNU C 函数库是一种类似于第三方插件的东西。由于 Linux 是用 C 语言写的，所以 Linux 的一些操作是用 C 语言实现的，因此，GUN 组织开发了一个 C 语言的库   以便让我们更好的利用 C 语言开发基于 Linux 操作系统的程序。

如今，`glibc `是 linux 下面 c 标准库的实现，即 **GNU C Library**，所以，我们在 `Linux` 用户态编程中最常用的库，`libc.so`，其实就是 `glibc`。

可能有人会认为，glib 前面有个 "g" ，所以认为 glib 是 GNU 的东西；同时认为 glibc 是 glib 的一个子集。  **其实，glib 和 glibc 基本上没有太大联系，可能唯一的共同点就是，其都是 C 编程需要调用的库而已。** 

> glib是GTK+的基础库，它由基础类型、对核心应用的支持、实用功能、数据类型和对象系统五个部分组成，可以在[http://www.gtk.org gtk网站]下载其源代码。是一个综合用途的实用的轻量级的C程序库，它提供C语言的常用的数据结构的定义、相关的处理函数，有趣而实用的宏，可移植的封装和一些运行时机能，如事件循环、线程、动态调用、对象系统等的API。GTK+是可移植的，当然glib也是可移植的，你可以在linux下，也可以在windows下使用它。使用gLib2.0（glib的2.0版本）编写的应用程序，在编译时应该在编译命令中加入pkg-config --cflags --libs glib-2.0
>

# 2 三个案例轻松搞定 glib 事件循环

## 2.1 案例1 g_main_loop 基础用法

此案例中，我们一共添加了 3 个事件，包括 2 个超时事件源，一个空闲函数。主循环 `g_main_loop_run` 不停地**检查是否有新事件**发生。**各个事件源处理函数，如果返回值为 FALSE，则该事件源会被删除。如果返回值为 TRUE，则事件源一直存在。**具体含义请看如下代码

```c
/* test.c */
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
```

每个事件函数的定义如下

```c
/* 
 * FALSE，该事件源将被删除
 * TRUE，该事件源会在没有更高优先级事件时，再次运行
 */
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

gboolean say_idle(gpointer data) {
    printf(">>> say_idle() idle \n");
    return TRUE;
}
```

编译

```shell
gcc $(pkg-config --cflags glib-2.0) test.c $(pkg-config --libs glib-2.0) -o test
```

或者使用 `Makefile`

运行结果如下

```shell
>>> say_idle() idle 
>>> count_down()   10
>>> count_down()    9
>>> count_down()    8
>>> count_down()    7
>>> count_down()    6
>>> count_down()    5
>>> count_down()    4
>>> cancel_fire() quit
```

结果分析

- 本例中，没有更高优先级的事件，所以首先会运行**空闲事件源**，空闲函数返回 `FALSE	`，该事件源被删除
- 接着运行**超时事件源**，两个超时函数同时处理，`count_down` 1秒运行一次，`cancel_fire` 8 秒运行一次

## 2.2 案例2 glib 自定义事件源

使用函数 `g_main_loop_new` 创建 `GSource`类型对象，这个对象可以理解为我们自定义事件源。自定义事件源需要完成以下函数的定义

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210514224400779.gif#pic_center)
事件循环中的关键函数

- **prepare**：调用每个事件源的 prepare 回调函数，检查事件源是否有事件发生。事件源分为两类：1、不需要 `poll`（idle 事件源），返回值 `TRUE`，表示 idle 事件已经发生；2、需要 poll（文件事件源），返回值 `FALSE`，因为只有轮询文件之后，才能知道文件事件是否发生。
- **query**：获取需要 `poll` 的文件 `fd`
- **check**：检查事件源是否有事件发生
- **dispatch**：若某个特定事件源有事件发生（通常是 `prepare/check`返回 `TRUE`），则调用事件处理函数

入口函数如下

```c
int main(int argc, char const *argv[]) {
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    GMainContext *context = g_main_loop_get_context(loop);

    GSourceFuncs g_source_myidle_funcs = {
        g_source_myidle_prepare,
        g_source_myidle_check,
        g_source_myidle_dispatch,
        g_source_myidle_finalize,
    };

    /* 创建新事件源实例，传入了事件的函数表、事件结构体大小 */
    GSource *source = g_source_new(&g_source_myidle_funcs, sizeof(GSourceMyIdle));
    /* 设置新事件源source的回调函数 */
    g_source_set_callback(source, (GSourceFunc)myidle, "Hello, world!", NULL);
    /* source关联特定的GMainContext对象 */
    g_source_attach(source, context);
    g_source_unref(source);

    g_main_loop_run(loop);

    g_main_context_unref(context);
    g_main_loop_unref(loop);

    return 0;
}
```

各个函数的定义

```c
/* 检查事件源是否有事件发生，如果有事件已经发生，则无需轮询（poll） */
gboolean g_source_myidle_prepare(GSource *source, gint *timeout) {
    *timeout = 0;
    return TRUE;
}

/* 检查事件源是否有事件发生 */
gboolean g_source_myidle_check(GSource *source) {
    return TRUE;
}

/* 分发事件源的事件=>调用事件源的回调函数 */
gboolean g_source_myidle_dispatch(GSource *source, GSourceFunc callback, gpointer user_data) {
    gboolean again;

    /* g_source_set_callback未设置 */
    if (!callback) {
        g_warning("no callback");
        return FALSE;
    }

    again = callback(user_data);

    return again;
}

/* source被销毁前调用，使用此函数释放资源 */
gboolean g_source_myidle_finalize(GSource *source) {
    return FALSE;
}

gboolean myidle(gchar *message) {
    g_print("%s\n", message);

    /*  
     * G_SOURCE_REMOVE, 删除事件源
     * G_SOURCE_CONTINUE, 保留事件源
     */
    return G_SOURCE_REMOVE;
}
```

运行结果

```C
Hello, world!
```

## 2.3 案例3 g_main_loop 自定义事件源
这里再介绍一个更加贴切实际的案例，这个案例中，我们启动一个 `TCP Server`
```shell
netcat -l 127.0.0.1 8888
```
我们的目标是使用 `glib` 事件循环，编写一个 `socket` 客户端，定义一个 `Echo` 事件源，接受 `TCP Server`  的数据。代码 `fork` 自 *KernelNewbies* 微信公众号提供的 `glib_demo`
```c
#include <stdio.h>
#include <glib.h>

#include <arpa/inet.h>
#include <sys/socket.h>

/* GSourceEcho */
typedef struct GSourceEcho {
	GSource source;
	GIOChannel *channel;
	GPollFD fd;
} GSourceEcho;

gboolean g_source_echo_prepare(GSource * source, gint * timeout);
gboolean g_source_echo_check(GSource * source);
gboolean g_source_echo_dispatch(GSource * source,
				GSourceFunc callback, gpointer user_data);
void g_source_echo_finalize(GSource * source);

gboolean echo(GIOChannel * channel);

/* tcp client */
int client_fd;

int client(void);

int main(int argc, char *argv[])
{
	if (-1 == client()) {
		g_print("%s\n", "start client error");

		return -1;
	}

	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	GMainContext *context = g_main_loop_get_context(loop);

	GSourceFuncs g_source_echo_funcs = {
		g_source_echo_prepare,
		g_source_echo_check,
		g_source_echo_dispatch,
		g_source_echo_finalize,
	};

	GSource *source =
	    g_source_new(&g_source_echo_funcs, sizeof(GSourceEcho));
	GSourceEcho *source_echo = (GSourceEcho *) source;

	source_echo->channel = g_io_channel_unix_new(client_fd);
	source_echo->fd.fd = client_fd;
	source_echo->fd.events = G_IO_IN;
	g_source_add_poll(source, &source_echo->fd);

	g_source_set_callback(source, (GSourceFunc) echo, NULL, NULL);

	g_source_attach(source, context);
	g_source_unref(source);

	g_main_loop_run(loop);

	g_main_context_unref(context);
	g_main_loop_unref(loop);

	return 0;
}

/* 对于文件描述符源，prepare 通常返回 FALSE，
 * 因为它必须等调用 poll 后才能知道是否需要处理事件
 *
 * 返回的超时为 -1，表示它不介意 poll 调用阻塞多长时间
*/
gboolean g_source_echo_prepare(GSource * source, gint * timeout)
{
	*timeout = -1;

	return FALSE;
}

/* 测试 poll 调用的结果，以查看事件是否发生
 */
gboolean g_source_echo_check(GSource * source)
{
	GSourceEcho *source_echo = (GSourceEcho *) source;

	/* events 为要监听的事件
	 * revents 为 poll 监听到的事件
	 *
	 * 因为只监听了一个事件，因此简化了比较方式
	 */
	if (source_echo->fd.revents != source_echo->fd.events) {
		return FALSE;
	}

	return TRUE;
}

gboolean g_source_echo_dispatch(GSource * source,
				GSourceFunc callback, gpointer user_data)
{
	gboolean again = G_SOURCE_REMOVE;

	GSourceEcho *source_echo = (GSourceEcho *) source;

	if (callback) {
		again = callback(source_echo->channel);
	}

	return again;
}

/* 释放 source 持有的资源的应用计数
 * 然后 source 才可以被安全地销毁，不会造成内存泄露
 */
void g_source_echo_finalize(GSource * source)
{
	GSourceEcho *source_echo = (GSourceEcho *) source;

	if (source_echo->channel) {
		g_io_channel_unref(source_echo->channel);
	}
}

gboolean echo(GIOChannel * channel)
{
	gsize len = 0;
	gchar *buf = NULL;

	g_io_channel_read_line(channel, &buf, &len, NULL, NULL);

	if (len > 0) {
		g_print("%s", buf);
		g_io_channel_write_chars(channel, buf, len, NULL, NULL);
		g_io_channel_flush(channel, NULL);
	}

	g_free(buf);

	return TRUE;
}

int client(void)
{
	struct sockaddr_in serv_addr;

	if (-1 == (client_fd = socket(AF_INET, SOCK_STREAM, 0))) {
		return -1;
	}

	memset(&serv_addr, 0, sizeof serv_addr);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serv_addr.sin_port = htons(9999);

	if (-1 ==
	    connect(client_fd, (struct sockaddr *) &serv_addr,
		    sizeof serv_addr)) {
		return -1;
	}

	return 0;
}
```

# 3 总结
glib 提供的事件循环机制非常实用，如 **GNOME、QEMU** 就使用了 glib 事件循环。本文只是轻度分析和使用了其提供的 API。更多深入的用法，其实只要结合一些开源项目，或者直接查看 glib 的源码，就可以了。