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
