/**
 * History:
 * ================================================================
 * 2018-10-22 qing.zou created
 *
 */
#ifndef TPC_ENDPOINTS_H
#define TPC_ENDPOINTS_H

#include "tpc_typedef.h"

TPC_BEGIN_DELS


typedef struct tpc_endpoints_data
{
	int			conn_id;
	int			status;

} tpc_endpoints_data;

typedef struct tpc_endpoints_op
{
	void (*on_register_status)(void *ctx, void *param);

	void (*on_connect_success)(void *ctx, void *param);

	void (*on_connect_failure)(void *ctx, void *param);

	void (*on_sock_disconnect)(void *ctx, void *param);

	void (*on_socket_clearing)(void *ctx, void *param);

	void (*on_socket_writable)(void *ctx, void *param);

	void (*on_receive_message)(void *ctx, void *pkt, size_t bytes_read);
} tpc_endpoints_op;

TPCAPI int tpc_endpoints_init(void *uid, int device);

int tpc_endpoints_register(tpc_endpoints_op *callback);

int tpc_endpoints_connect(void *uid);

int tpc_packet_send(const void *pkt, size_t size);

void tpc_endpoints_disconnect(void);

int tpc_endpoints_destroy(void);

void tpc_endpoints_status(void);

int tpc_thread_register(const char *thread_name);

int tpc_thread_is_registered(void);

TPC_END_DELS

#endif //TPC_ENDPOINTS_H
