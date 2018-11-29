/**
 * History:
 * ================================================================
 * 2018-10-09 qing.zou created
 *
 */
#ifndef TPC_TYPEDEF_H
#define TPC_TYPEDEF_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#define TPC_BEGIN_DELS extern "C" {
#define TPC_END_DELS }
#else
#define TPC_BEGIN_DELS
#define TPC_END_DELS
#endif

#ifndef return_if_fail
#define return_if_fail(p) if(!(p)) \
	{printf("%s:%d Error: "#p" failed.\n", __FILE__, __LINE__); \
	(void)fprintf(stderr, "%s:%d Error: "#p" failed.\n", \
	__FILE__, __LINE__); return;}
#define return_val_if_fail(p, ret) if(!(p)) \
	{printf("%s:%d Error: "#p" failed.\n", __FILE__, __LINE__); \
	(void)fprintf(stderr, "%s:%d Error: "#p" failed.\n", \
	__FILE__, __LINE__); return (ret);}
#endif

#ifndef DECL_PRIV
  #define DECL_PRIV(thiz, priv) PrivInfo* priv = thiz != NULL ? (PrivInfo*)thiz->priv : NULL
#endif

#ifndef INLINE
#define INLINE __inline__
#else
//#define INLINE
#endif

#define	TPC_MALLOC		malloc
#define	TPC_CALLOC		calloc
#define	TPC_FREE		free
#define	TPC_MEMSET		memset
#define	TPC_MEMCPY		memcpy
#define	TPC_REALLOC		realloc

#define TPCAPI			extern	

//#define TPC_CONFIG_ANDROID 0

#if defined(TPC_CONFIG_ANDROID) && TPC_CONFIG_ANDROID==1

  #include <android/log.h>
  #define LOG_TAG "tpc"
  #define TPC_LOGD(x)	XXX_LOGD x
  #define XXX_LOGD(format, args...) 	__android_log_print(ANDROID_LOG_DEBUG,LOG_TAG, format, ##args)
  #define TPC_LOGI(format, args...) 	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, format, ##args)
  #define TPC_LOGW(format, args...) 	__android_log_print(ANDROID_LOG_WARN, LOG_TAG, format, ##args)
  #define TPC_LOGE(format, args...) 	__android_log_print(ANDROID_LOG_ERROR,LOG_TAG, format, ##args)
  #define TPC_LOGF(format, args...) 	__android_log_print(ANDROID_LOG_FATAL,LOG_TAG, format, ##args)

#else
#ifdef USE_ZLOG
#include "zlog.h"
/* use zlog */
#ifndef TIMA_LOGI
/* tima log macros */
#define TIMA_LOGF(format, args...) \
	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ZLOG_LEVEL_FATAL, format, ##args)
#define TIMA_LOGE(format, args...) \
	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ZLOG_LEVEL_ERROR, format, ##args)
#define TIMA_LOGW(format, args...) \
	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ZLOG_LEVEL_WARN, format, ##args)
#define TIMA_LOGN(format, args...) \
	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ZLOG_LEVEL_NOTICE, format, ##args)
#define TIMA_LOGI(format, args...) \
	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ZLOG_LEVEL_INFO, format, ##args)
#define TIMA_LOGD(format, args...) \
	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ZLOG_LEVEL_DEBUG, format, ##args)
#endif

#define TPC_LOGD(x)	XXX_LOGD x
#define XXX_LOGD(format, args...) 	TIMA_LOGD(format, ##args)
#define TPC_LOGI(format, args...) TIMA_LOGI(format, ##args)
#define TPC_LOGW(format, args...) TIMA_LOGW(format, ##args)
#define TPC_LOGE(format, args...) TIMA_LOGE(format, ##args)
#define TPC_LOGF(format, args...) TIMA_LOGF(format, ##args)
#else
	#if 0
	#define TPC_LOGD(x)	TPC_LOGD x
	#define TPC_LOGI		TPC_LOGI
	#define TPC_LOGW		TPC_LOGW
	#define TPC_LOGE		TPC_LOGE
	#define TPC_LOGF		TPC_LOGF
	#else
	#define TPC_LOGD(x)	printf
	#define TPC_LOGI		printf
	#define TPC_LOGW		printf
	#define TPC_LOGE		printf
	#define TPC_LOGF		printf
	#endif

#endif

#endif //TPC_CONFIG_ANDROID

#endif //TPC_TYPEDEF_H
