/**
 * History:
 * ================================================================
 * 2017-08-21 qing.zou created
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
//#include <sys/time.h>

#include <fcntl.h>
#include <sys/un.h>
#include <errno.h>

#include "tpc_bufferev.h"

static int time_sub(struct timeval *next, struct timeval *prev, struct timeval *result)
{
	if (prev->tv_sec > next->tv_sec) return -1;
	if (prev->tv_sec == next->tv_sec && prev->tv_usec > next->tv_usec) return -1;

	result->tv_sec = next->tv_sec - prev->tv_sec;
	result->tv_usec = next->tv_usec - prev->tv_usec;
	if (result->tv_usec < 0)
	{
		result->tv_sec--;
		result->tv_usec += 1000000;
	}

	return 0;
}
static void safe_flush(FILE *fp)
{
	int ch;
	while( (ch = fgetc(fp)) != EOF && ch != '\n' );
}

#ifdef USE_ZLOG
#define SAMPLE_ZLOGFILE_PATH		"."
#define SAMPLE_ZLOGCONF_FILE		"./zlog.conf"
int sample_zlog_init(int procname)
{
	int rc;
	//zlog_category_t *c;

	//if (!tpc_exists(SAMPLE_ZTPC_LOGFILE_PATH)) {
	//	int ret = 0;
	//	char tmp[256] = {0};
	//	tpc_pathname_get(SAMPLE_ZTPC_LOGFILE_PATH, tmp);
	//	printf("full: %s, pathname: %s", SAMPLE_ZTPC_LOGFILE_PATH, tmp);
	//	ret = tpc_mkdir_mult(SAMPLE_ZTPC_LOGFILE_PATH);
	//	printf("tpc_mkdir_mult \'%s\' ret = %d\n", SAMPLE_ZTPC_LOGFILE_PATH, ret);
	//}

	rc = dzlog_init(SAMPLE_ZLOGCONF_FILE, "sample");
	if (rc)	{
		printf("zlog init failed\n");
		return -1;
	}

	TPC_LOGI("hello, zlog");

	return 0;
}
#endif

static void cmd_msg_cb(int fd, short events, void *arg);
static void server_msg_cb(tpc_bufferev_t *bev, void *arg);
static void event_cb(tpc_bufferev_t *bev, short event, void *arg);

tpc_evbase_t *base = NULL;
tpc_bufferev_t *bev = NULL;
static int base_initialized = 0;

static int client_main(char *ip, int port) {

	base = tpc_evbase_create();
	base_initialized = 1;

	//tpc_bufferev_t *bev = tpc_bufferev_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
	bev = tpc_bufferev_channel_new(base, 1, 1);

	//tpc_events *ev_cmd = tpc_event_new(base, STDIN_FILENO, TPC_EV_READ|TPC_EV_PERSIST, cmd_msg_cb, (void *)bev);

	//tpc_event_add(ev_cmd, NULL);

	//tpc_bufferev_channel_connect(bev, (struct sockaddr *)&server_addr, sizeof(server_addr));
	//tpc_bufferev_setcb(bev, server_msg_cb, NULL, event_cb, (void *)ev_cmd);
	//tpc_bufferev_enable(bev, EV_READ|EV_PERSIST);

	//event_base_dispatch(base);
	tpc_evbase_loop(base, 0);

	printf("Finished\n");
	tpc_evbase_destroy(base);
	return 0;

}

static void cmd_msg_cb(int fd, short events, void *arg) {
	char msg[1024];

	int ret = read(fd, msg, sizeof(msg));
	if (ret < 0) {
		perror("read fail.\n");
		exit(1);
	}
	if (strncasecmp(msg, "exit", strlen("exit")) == 0)
	{
		tpc_bufferev_free(arg);
		tpc_evbase_loopbreak(base);
		return;
	}

	printf("write(%d): %s\n", ret, msg);
	tpc_bufferev_t *bev = (tpc_bufferev_t *)arg;
	tpc_bufferev_write(bev, msg, ret);
}

static void server_msg_cb(tpc_bufferev_t *bev, void *arg) {
	char msg[1024];

	//size_t len = tpc_bufferev_read(bev, msg, sizeof(msg)-1);
	//msg[len] = '\0';

	printf("Recv %s from server.\n", msg);
}

static void event_cb(tpc_bufferev_t *bev, short event, void *arg) {
	if (event & TPC_BEV_EVENT_EOF) {
		printf("Connection closed.\n");
	}
	else if (event & TPC_BEV_EVENT_ERROR) {
		printf("Some other error.\n");
	}
	else if (event & TPC_BEV_EVENT_CONNECTED) {
		printf("Client has successfully cliented.\n");
		return;
	}

	tpc_bufferev_free(bev);

	// free event_cmd
	//tpc_events *ev = (tpc_events *)arg;
	//tpc_event_free(ev);
}


tpc_events *events_notice2;
tpc_events *events_notice3;
static struct timeval prev;
static void notice_callback2(int fd, short event, void *arg) 
{
	if (event & TPC_EV_TIMEOUT)
		TPC_LOGI("timeout notice_callback2.");
	TPC_LOGI("%p ========= 2222fd(%d) event(0x%02x) in notice_callback2 =========", arg, fd, event);
	//start_timer_flag = 1;
	//tpc_event_free(events_notice2);

	double elapsed;
	struct timeval nowtime, difference;

	gettimeofday(&nowtime, NULL);
	//vpk_timersub(&nowtime, &prev, &difference);
	time_sub(&nowtime, &prev, &difference);
	elapsed = difference.tv_sec + (difference.tv_usec / 1.0e6);
	prev = nowtime;

	TPC_LOGI("time elapsed:  %d, %.6f seconds elapsed.", nowtime.tv_sec, elapsed);
	TPC_LOGI("time elapsed: %.6f, %d(s) %d(us) \n", elapsed, difference.tv_sec, difference.tv_usec);
}
static void notice_callback3(int fd, short event, void *arg) 
{
	if (event & TPC_EV_TIMEOUT)
		TPC_LOGI("timeout notice_callback3.");
	TPC_LOGI("%p ========= 3333fd(%d) event(0x%02x) in notice_callback3 =========", arg, fd, event);

	char *data = tpc_event_data_get(events_notice3);
	TPC_LOGI("recv data: %s", data);

	tpc_event_free(events_notice3);
}

int msg_event_add(const char* name)
{
	int ret = -1;
	// events_notice2
	events_notice2 = tpc_event_new(base, 2, TPC_EV_NOTICE|TPC_EV_PERSIST, notice_callback2, base);
	tpc_event_add(events_notice2, NULL);
	sleep(1);
	// events_notice3
	//events_notice3 = tpc_event_new(base, 3, TPC_EV_NOTICE|TPC_EV_MOMENT|TPC_EV_PERSIST, notice_callback3, base);
	//tpc_event_add(events_notice3, NULL);

	return ret;
}

static int trigger_event1(int code)
{
	int ret = -1;
	//while (1)
	{
		TPC_LOGI("code = 0x%x\n", code);

		if ((ret = tpc_evmsg_notice(base, code, 0, 0)) < 0) {
			TPC_LOGE("tpc_evmsg_notice failed, maybe msg not register callback.");
		}

		sleep(1);
	}


	return ret;
}

// test..
FILE*				fp;
size_t				offset;
int					file_size;
unsigned			buf_size;
char				*buf;
int cnt = 0;
static int vpk_file_open(const char* filename)
{
	size_t size = 1024;

	int result;
	fp = fopen(filename, "r");
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buf_size = (offset + size) <= file_size ? size : file_size;
	buf = malloc(buf_size);
	return 0;
}
static int vpk_file_read()
{
	int result;

	if (offset < file_size) {
		memset(buf, 0x00, buf_size);
		fseek(fp, offset, SEEK_SET);
		result = fread(buf, 1, buf_size, fp);
		if (result < 0) {
			printf("=======read file end!\n");
			return -1;
		}
		offset += result;

		//printf("%05d ", cnt++);
		printf("%d[%05d] ", result, cnt++);

		//tpc_packet_send(buf, result);
		//printf("write(%d): %s\n", result, msg);
		tpc_bufferev_write(bev, buf, result);

		return result;
	} else {
		printf("\n=========== read end!!\n");
		if (fp) 
			fclose(fp);

	}
	printf("\n===========111111 read end!!\n");

	return -1;
}
void *tpc_test1(void* arg)
{
	while (!base_initialized) {
		sleep(1);
	}
	TPC_LOGI("test1 thread run.");
	char ch = '0';
	printf("\n\n\n");

#if 1

	vpk_file_open("./test.tmp");

	static struct timeval start;
	gettimeofday(&start, NULL);
	while (1)
	{
		int ret = vpk_file_read();
		if (ret < 0) {

			while (tpc_evbuffer_get_length(bev->output) != 0)
				usleep(200000);

			double elapsed;
			struct timeval nowtime, difference;
			gettimeofday(&nowtime, NULL);
			time_sub(&nowtime, &start, &difference);
			elapsed = difference.tv_sec + (difference.tv_usec / 1.0e6);
			start = nowtime;
			TPC_LOGI("time elapsed:  %d, %.6f seconds elapsed.", nowtime.tv_sec, elapsed);

			printf("read file size: %d\n", file_size);

			tpc_bufferev_free(bev);
			tpc_evbase_loopbreak(base);
			return NULL;
		}
		usleep(1000);

	}

	while (0)
	{
		sleep(1);
		printf("\nplease enter msg:\n");

		char msg[1024];

		//int ret = read(STDIN_FILENO, msg, sizeof(msg));
		int ret = read(fileno(stdin), msg, sizeof(msg));
		if (ret < 0) {
			perror("read fail.\n");
			exit(1);
		}
		if (strncasecmp(msg, "exit", strlen("exit")) == 0)
		{
			tpc_bufferev_free(bev);
			tpc_evbase_loopbreak(base);
			return NULL;
		}

		printf("write(%d): %s\n", ret, msg);
		tpc_bufferev_write(bev, msg, ret);

	}

#else
	msg_event_add(NULL);
	while (1)
	{
		sleep(1);
		TPC_LOGI("please press 'm' to trigger event\n");

		printf("\n\n\nEnter code: ");
		scanf("%c", &ch);
		safe_flush(stdin);
		printf("ch = %c\n\n", ch);
		switch (ch) {
			case 'm':
				trigger_event1(2);
				break;
			default:break;
		}

	}

#endif
	return NULL;
}

void *tpc_main1(void* arg)
{
	do 
	{
		TPC_LOGI("main thread run.");
		client_main(NULL, 0);
	} while (0);
	TPC_LOGI("tpc_main1 exit.");
}

int main(int argc, char *argv[])
{
	int ret = 0;
	void* thread_result;
	pthread_t pth_test3, pth_test1, pth_main2, pth_main1;

#ifdef USE_ZLOG
	sample_zlog_init(0);
#endif // USE_ZLOG

	//tpc_system_init(argc, argv);
	//tpc_logging_level_set("DEBUG");

	ret = pthread_create(&pth_test1, NULL, tpc_test1, (void*)NULL);
	if (ret != 0)
		TPC_LOGE("create thread \'tpc_test2\' failed");

	ret = pthread_create(&pth_main1, NULL, tpc_main1, (void*)NULL);
	if (ret != 0)
		TPC_LOGE("create thread \'tpc_main1\' failed");

	pthread_join(pth_test1, &thread_result);
	pthread_join(pth_main1, &thread_result);

	return 0;
}
