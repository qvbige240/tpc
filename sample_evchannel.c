/**
 * History:
 * ================================================================
 * 2017-12-08 qing.zou created
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "tpc_events.h"

//#define MULTI_EVENT_BASE_SUPPORT

void *tpc_test1(void* arg);
//void *tpc_test2(void* arg);
void *tpc_test3(void* arg);
void *tpc_main1(void* arg);

#ifdef MULTI_EVENT_BASE_SUPPORT
void *tpc_main2(void* arg);
#endif

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


	ret = pthread_create(&pth_test3, NULL, tpc_test3, (void*)NULL);
	if (ret != 0)
		TPC_LOGE("create thread \'tpc_test3\' failed");

	//ret = pthread_create(&pth_test2, NULL, tpc_test2, (void*)NULL);
	//if (ret != 0)
	//	TPC_LOGE("create thread \'tpc_test2\' failed");

	ret = pthread_create(&pth_test1, NULL, tpc_test1, (void*)NULL);
	if (ret != 0)
		TPC_LOGE("create thread \'tpc_test2\' failed");

	ret = pthread_create(&pth_main1, NULL, tpc_main1, (void*)NULL);
	if (ret != 0)
		TPC_LOGE("create thread \'tpc_main1\' failed");

#ifdef MULTI_EVENT_BASE_SUPPORT
	ret = pthread_create(&pth_main2, NULL, tpc_main2, (void*)NULL);
	if (ret != 0)
		TPC_LOGE("create thread \'tpc_main2\' failed");

	pthread_join(pth_main2, &thread_result);
#endif

	pthread_join(pth_test1, &thread_result);
	pthread_join(pth_test3, &thread_result);
	//pthread_join(pth_test2, &thread_result);
	pthread_join(pth_main1, &thread_result);

	return 0;
}

//static int start = 0;
tpc_evbase_t *base = NULL;

#ifdef MULTI_EVENT_BASE_SUPPORT
tpc_evbase_t *base2 = NULL;
#endif

tpc_events *events_time;
static struct timeval prev;
int called = 0;

static void test_timer_callback(int fd, short event, void *arg)
{
	//int value = HEART_BEAT_MESSAGE_VALUE;
	double elapsed;
	struct timeval nowtime, difference;

	tpc_gettimeofday(&nowtime, NULL);
	tpc_timersub(&nowtime, &prev, &difference);
	elapsed = difference.tv_sec + (difference.tv_usec / 1.0e6);
	prev = nowtime;

	TPC_LOGI("test_timer_event, at %d: %.6f seconds elapsed. %p", nowtime.tv_sec, elapsed, &test_timer_callback);

	if (called >= 2)
		tpc_event_free(events_time);

	called++;
}

static int timer_event_add(const char* name)
{
	int ret = -1;
	TPC_LOGI("sample add a timer\n");
	events_time = tpc_event_new(base, 0, TPC_EV_PERSIST, test_timer_callback, NULL);;

	struct timeval tv;
	tpc_timerclear(&tv);
	tv.tv_sec = 5;
	tpc_gettimeofday(&prev, NULL);
	tpc_event_add(events_time, &tv);

	// need free events_time somewhere
	// tpc_event_free(events_time);
	return ret;
}

#if 0
static int eventq_recv(const char* name)
{
	int ret = -1;
	tpc_event_t alert = {0};
	tpc_eventq_t* eventq = NULL;
	return_val_if_fail(name != NULL, -1);

	eventq = tpc_eventq_open("/test", "a+");
	return_val_if_fail(eventq != NULL, -1);

	TPC_LOGI("recv queue start");
	while (1)
	{
		memset(&alert, 0x00, sizeof(tpc_event_t));
		ret = tpc_eventq_recv(eventq, &alert);
		TPC_LOGI("[%s] ret = %d, recv event key = 0x%x\n", name, ret, alert.alert.keycode);

		if ((ret = tpc_evmsg_notice(alert.alert.keycode, alert.alert.data, strlen(alert.alert.data))) < 0) {
			TPC_LOGE("tpc_evmsg_notice failed, maybe msg not register callback.");
		}

		sleep(1);
	}

	tpc_eventq_close(eventq);
	tpc_eventq_destroy(eventq);

	return ret;
}
#else
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
  #ifdef MULTI_EVENT_BASE_SUPPORT
	static int trigger_event2(int code)
	{
		int ret = -1;
		//while (1)
		{
			TPC_LOGI("code = 0x%x\n", code);

			if ((ret = tpc_evmsg_notice(base2, code, 0, 0)) < 0) {
				TPC_LOGE("tpc_evmsg_notice failed, maybe msg not register callback.");
			}

			sleep(1);
		}


		return ret;
	}
  #endif
#endif

tpc_events *events_notice;
tpc_events *events_notice2;
tpc_events *events_notice3;
static int start_timer_flag = 0;
static void notice_callback(int fd, short event, void *arg) 
{
	if (event & TPC_EV_TIMEOUT)
		TPC_LOGI("timeout notice_callback.");
	TPC_LOGI("%p =========1111fd(%d) event(0x%02x) in notice_callback.", arg, fd, event);
	//start_timer_flag = 1;
	tpc_event_free(events_notice);
}

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
	
#ifdef MULTI_EVENT_BASE_SUPPORT
	events_notice = tpc_event_new(base2, 1, TPC_EV_NOTICE|TPC_EV_PERSIST, notice_callback, base2);
	tpc_event_add(events_notice, NULL);
#endif

	//struct timeval tv;
	//tpc_timerclear(&tv);
	//tv.tv_sec = 10;
	//tpc_event_add(events_notice, &tv);

	// events_notice2
	events_notice2 = tpc_event_new(base, 2, TPC_EV_NOTICE|TPC_EV_PERSIST, notice_callback2, base);
	tpc_event_add(events_notice2, NULL);
	sleep(1);
	// events_notice3
	events_notice3 = tpc_event_new(base, 3, TPC_EV_NOTICE|TPC_EV_MOMENT|TPC_EV_PERSIST, notice_callback3, base);
	tpc_event_add(events_notice3, NULL);

	return ret;
}

static void pipe_callback(int fd, short event, void *arg) {
	TPC_LOGI("in pipe_callback.");
}

static int base_initialized = 0;
int msg_event_main1(const char* name)
{
	int ret = -1;
	tpc_events events_pipe;
	return_val_if_fail(name != NULL, -1);

	base = tpc_evbase_create();
	
	base_initialized = 1;
	//int pipe_fd[2];
	//pipe(pipe_fd);
	//TPC_LOGD(("pipe_fd 0 1: %d %d", pipe_fd[0], pipe_fd[1]));

	//tpc_event_assign(&events_pipe, base, pipe_fd[0], TPC_EV_READ|TPC_EV_PERSIST, pipe_callback, NULL);

	//tpc_event_add(&events_pipe, NULL);

	tpc_evbase_loop(base, 0);

	return ret;
}

#ifdef MULTI_EVENT_BASE_SUPPORT
int msg_event_main2(const char* name)
{
	int ret = -1;
	return_val_if_fail(name != NULL, -1);

	base2 = tpc_evbase_create();
	tpc_evbase_loop(base2, 0);

	return ret;
}
#endif

#if 0
void *tpc_test1(void* arg)
{
	while(1)
	{
		TPC_LOGI("test1 thread run.");
		eventq_recv("RECV EVENTQ");
		sleep(1);
	}

	return NULL;
}
#else
void *tpc_test1(void* arg)
{
	while (!base_initialized) {
		sleep(1);
	}
	TPC_LOGI("test1 thread run.");
	char ch = '0';
	printf("\n\n\n");

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
#ifdef MULTI_EVENT_BASE_SUPPORT
			case 'n':
				trigger_event2(1);
				break;
#endif
			default:break;
		}

	}

	return NULL;
}
#endif

void *tpc_test3(void* arg)
{
	while (!base_initialized) {
		sleep(1);
	}
	TPC_LOGI("test3 thread run.");
	char ch = '0';
// 	printf("\n\n\n");
// 	TPC_LOGI("please press 's' to start to register a msg event");
// 	while (ch != 's') {
// 		scanf("%c", &ch);
//	}
// 	TPC_LOGI("ch = %c", ch);
// 	printf("\n\n\n");

	msg_event_add(NULL);

// 	printf("\n\n\n");
// 
// 	while (!start_timer_flag) {
// 		sleep(3);
// 	}
// 
// 	printf("\n\n\n");
// 	timer_event_add(NULL);
// 	printf("\n\n\n");

	return NULL;
}

void *tpc_main1(void* arg)
{
	//sleep(1);
	//TPC_LOGI("start test2 thread!");
	while(1)
	{
		TPC_LOGI("main thread run.");
		msg_event_main1("main1");
		sleep(1);
	}
}

#ifdef MULTI_EVENT_BASE_SUPPORT
void *tpc_main2(void* arg)
{
	//sleep(1);
	//TPC_LOGI("start test2 thread!");
	while(1)
	{
		TPC_LOGI("main2 thread run.");
		msg_event_main2("main2");
		sleep(1);
	}
}
#endif
