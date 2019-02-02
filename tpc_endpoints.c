/**
 * History:
 * ================================================================
 * 2018-10-22 qing.zou created
 *
 */

#include "ice_client.h"

#include "tpc_endpoints.h"
#include "tpc_bufferev.h"
#ifdef SIP_SERVER_DEVELOP_ENV
#define SERVER_INTERNAL_NETWORK		"172.17.13.8"#define SERVER_EXTERNAL_NETWORK		"222.209.88.97"
#define SERVER_REALM				"172.17.13.8"
#elif SIP_SERVER_TEST_ENV
#define SERVER_INTERNAL_NETWORK		"172.20.25.40"
//#define SERVER_EXTERNAL_NETWORK		"115.182.105.80"
#define SERVER_EXTERNAL_NETWORK		"p2ptest.91carnet.com"
#define SERVER_REALM				"91carnet.com"
#endif

int tpc_endpoints_init(void *uid, int device)
{
	int ret = 0;

	ice_info_t info = {0};
	char account[64] = {0};

	if (device)
		sprintf(account, "%sA", (char*)uid);		// DEVICE: uid+A
	else
		sprintf(account, "%sB", (char*)uid);		// ANDROID/IOS: uid+B

	char *server = NULL;

#ifdef SIP_SERVER_INTERNAL
	server = SERVER_INTERNAL_NETWORK;
#else
	server = SERVER_EXTERNAL_NETWORK;
#endif

	strcpy(info.account, account);
	strcpy(info.passwd, account);
	strcpy(info.server, server);
	strcpy(info.realm, SERVER_REALM);

	strcpy(info.turn, server);
	strcpy(info.turn_port, "3488");
	if (device) {
		strcpy(info.username, account);
		strcpy(info.password, account);
	} else {
		strcpy(info.username, account);
		strcpy(info.password, account);
	}

// 	//char remote_uri[128] = {0};
// 	if (client == 1) {
// 		strcpy(info.account, "101");
// 		strcpy(info.passwd, "101");
// 		strcpy(info.server, server);
// 		strcpy(info.realm, "172.17.13.8");
// 
// 		strcpy(info.turn, server);
// 		strcpy(info.turn_port, "3488");
// 		strcpy(info.username, "username1");
// 		strcpy(info.password, "password1");
// 		//sprintf(remote_uri, "sip:102@%s", info.server);
// 	} else {
// 		strcpy(info.account, "102");
// 		strcpy(info.passwd, "102");
// 		strcpy(info.server, server);
// 		strcpy(info.realm, "172.17.13.8");
// 
// 		strcpy(info.turn, server);
// 		strcpy(info.turn_port, "3488");
// 		strcpy(info.username, "username2");
// 		strcpy(info.password, "password2");
// 		//sprintf(remote_uri, "sip:101@%s", info.server);
// 	}

	ret = ice_client_init(&info);
	if (ret != 0)
		return -1;

	return 0;
}

typedef struct _PrivInfo
{
	tpc_endpoints_op	cb;

	pj_pool_t			*pool;

	pj_thread_t			*thread;

	void				*parent;

	tpc_bufferev_t		*bev;

	int					is_device;

	int					reg_status;

	int					connected;
	int					eagain;

	//pthread_mutex_t		channel_mutex;
	//sem_t				channel_sem;
} PrivInfo;

static int base_dispatch_thread(void *args);
int tpc_endpoints_create(void *uid, int device, void *parent, tpc_endpoints_op *cb, tpc_endpoints_t **endpoints)
{
	int ret = 0;
	tpc_endpoints_t *thiz;

	ret = tpc_endpoints_init(uid, device);
	if (ret != 0) 
		return ret;

	thiz = TPC_CALLOC(1, (sizeof(tpc_endpoints_t) + sizeof(PrivInfo)));
	if (thiz)
	{
		DECL_PRIV(thiz, priv);
		pj_status_t status = PJ_TRUE;
		pj_pool_t *pool = ice_pool_create("tpc-ev", 200, 1000);

		priv->pool		= pool;
		priv->parent	= parent;
		priv->is_device	= device;

		memcpy(&priv->cb, cb, sizeof(priv->cb));
		
		/* Create tpc base thread. */
		status = pj_thread_create(pool, "tpc-base", base_dispatch_thread, thiz, 0, 0, &priv->thread);
		if (status != PJ_SUCCESS) {
			//... destroy ice
			TPC_FREE(thiz);
			return -1;
		}

		*endpoints = thiz;
	}

	return ret;
}

static int base_dispatch_thread(void *args)
{
	tpc_endpoints_t *thiz = (tpc_endpoints_t*)args;

	//tpc_evbase_t *base = tpc_evbase_create();
	thiz->base = tpc_evbase_create();

	tpc_evbase_loop(thiz->base, 0);

	TPC_LOGI("tpc evbase loop exit!");
	tpc_evbase_destroy(thiz->base);

	printf("Finished\n");

	return 0;
}

#if 0  //demo
static void channel_writecb(tpc_bufferev_t *bev, void *arg) {
	printf("writecb.\n");
	//tpc_bufferev_enable(bev, TPC_EV_WRITE);
}
static void channel_eventcb(tpc_bufferev_t *bev, short event, void *arg) {
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
}
static int endpoints_main(tpc_endpoints_t *endpoint)
{
	char *uid = "tima";
	tpc_bufferev_t *bev = NULL;

	//tpc_endpoints_create();
	//tpc_endpoints_login();
	bev = tpc_bufferev_new(endpoint, 1);
	//bev = tpc_bufferev_channel_new(endpoint->base, 1, 1);

	tpc_bufferev_channel_connect(bev, endpoint, uid);
	tpc_bufferev_setcb(bev, NULL, channel_writecb, channel_eventcb, (void *)endpoint);
	tpc_bufferev_enable(bev, TPC_EV_WRITE|TPC_EV_PERSIST);

	return 0;
}
#endif

tpc_bufferev_t *tpc_bufferev_new(tpc_endpoints_t *endpoint, evutil_channel_t ch)
{
	tpc_endpoints_t *thiz = endpoint;
	DECL_PRIV(thiz, priv);
	tpc_bufferev_t *bev = NULL;

	bev = tpc_bufferev_channel_new(thiz->base, ch, 1);

	if (priv != NULL)
		priv->bev = bev;
	return bev;
}

int tpc_bufferev_channel_connect(tpc_bufferev_t *bev, tpc_endpoints_t *p, void *uid)
{
	tpc_endpoints_t *thiz = p;
	DECL_PRIV(thiz, priv);
	if (priv != NULL)
		priv->bev = bev;
	
	return tpc_endpoints_connect(uid);
}

static void on_register_status(void *ctx, void *param)
{
	tpc_endpoints_t *thiz = ctx;
	ice_client_param *p = param;
	tpc_endpoints_data data = {0};
	DECL_PRIV(thiz, priv);

	data.conn_id = p->call_id;
	data.status = p->status;
	priv->reg_status = p->status;
	if (priv->cb.on_register_status)
		priv->cb.on_register_status(priv->parent, &data);
}
static void on_connect_success(void *ctx, void *param)
{
	tpc_endpoints_t *thiz = ctx;
	ice_client_param *p = param;
	tpc_endpoints_data data = {0};
	DECL_PRIV(thiz, priv);

	data.conn_id = p->call_id;
	data.status = p->status;
	priv->connected = 1;
	if (priv->cb.on_connect_success)
		priv->cb.on_connect_success(priv->parent, &data);
}
static void on_connect_failure(void *ctx, void *param)
{
	tpc_endpoints_t *thiz = ctx;
	ice_client_param *p = param;
	tpc_endpoints_data data = {0};
	DECL_PRIV(thiz, priv);

	data.conn_id = p->call_id;
	data.status = p->status;
	if (priv->cb.on_connect_failure)
		priv->cb.on_connect_failure(priv->parent, &data);
}
static void on_sock_disconnect(void *ctx, void *param)
{
	tpc_endpoints_t *thiz = ctx;
	ice_client_param *p = param;
	tpc_endpoints_data data = {0};
	DECL_PRIV(thiz, priv);

	data.conn_id = p->call_id;
	data.status = p->status;
	priv->connected = 0;
	if (priv->cb.on_sock_disconnect)
		priv->cb.on_sock_disconnect(priv->parent, &data);
}
static void on_socket_clearing(void *ctx, void *param)
{
}
static void on_socket_writable(void *ctx, void *param)
{
	tpc_endpoints_t *thiz = ctx;
	DECL_PRIV(thiz, priv);

	if (priv && priv->bev)
		tpc_bufferev_enable(priv->bev, TPC_EV_WRITE);
}
static void on_receive_message(void *ctx, void *pkt, pj_ssize_t bytes_read)
{
	tpc_endpoints_t *thiz = ctx;
	DECL_PRIV(thiz, priv);
	if (priv && priv->cb.on_receive_message)
		priv->cb.on_receive_message(priv->parent, pkt, bytes_read);
}

int tpc_endpoints_register(tpc_endpoints_op *callback)
{
	iclient_callback *ctx = (iclient_callback *)callback;

	int status = ice_client_register(ctx);

	return status;
}
int tpc_endpoints_login(tpc_endpoints_t *endpoint)
{
	int status = 0;
	tpc_endpoints_t *thiz = endpoint;

	iclient_callback client_cb = {0};
	client_cb.on_register_status = on_register_status;
	client_cb.on_connect_success = on_connect_success;
	client_cb.on_connect_failure = on_connect_failure;
	client_cb.on_sock_disconnect = on_sock_disconnect;
	client_cb.on_socket_clearing = on_socket_clearing;
	client_cb.on_socket_writable = on_socket_writable;
	client_cb.on_receive_message = on_receive_message;
	status = ice_client_login(&client_cb, thiz);

	return status;
}

int tpc_endpoints_connect(void *uid)
{
	char remote_uri[128] = {0};

	char *server = NULL;

#ifdef SIP_SERVER_INTERNAL
	server = SERVER_INTERNAL_NETWORK;
#else
	server = SERVER_EXTERNAL_NETWORK;
#endif

	sprintf(remote_uri, "sip:%sA@%s", (char*)uid, server);

	return ice_make_connect(remote_uri);
}

int tpc_packet_send(const void *pkt, size_t size)
{
	return ice_packet_send(pkt, size);
}

void tpc_endpoints_disconnect(void)
{
	ice_client_disconnect();
}

int tpc_endpoints_destroy(tpc_endpoints_t *thiz)
{
	if (thiz) {
		DECL_PRIV(thiz, priv);

		tpc_bufferev_free(priv->bev);
		tpc_evbase_loopbreak(thiz->base);

		//pjthread
		//pool
	}

	return ice_client_destroy();
}

void tpc_endpoints_status(void)
{
	ice_client_status();
}

int tpc_thread_register(const char *thread_name)
{
	return ice_thread_register(thread_name);
}

int tpc_thread_is_registered(void)
{
	return ice_thread_is_registered();
}
