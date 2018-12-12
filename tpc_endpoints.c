/**
 * History:
 * ================================================================
 * 2018-10-22 qing.zou created
 *
 */

#include "ice_client.h"

#include "tpc_endpoints.h"
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
	pj_status_t status = PJ_TRUE;

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

int tpc_endpoints_register(tpc_endpoints_op *callback)
{
	iclient_callback *ctx = (iclient_callback *)callback;

	ice_client_register(ctx);

	return 0;
}

void tpc_endpoints_connect(void *uid)
{
	char remote_uri[128] = {0};

	char *server = NULL;

#ifdef SIP_SERVER_INTERNAL
	server = SERVER_INTERNAL_NETWORK;
#else
	server = SERVER_EXTERNAL_NETWORK;
#endif

	sprintf(remote_uri, "sip:%sA@%s", (char*)uid, server);

	ice_make_connect(remote_uri);
}

int tpc_packet_send(const void *pkt, size_t size)
{
	return ice_packet_send(pkt, size);
}

void tpc_endpoints_disconnect(void)
{
	ice_client_disconnect();
}

int tpc_endpoints_destroy(void)
{
	return ice_client_destroy();
}

void tpc_endpoints_status(void)
{
	ice_client_status();
}

int tpc_thread_register(const char *thread_name)
{
	ice_thread_register(thread_name);
}
