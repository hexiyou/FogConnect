
#include <assert.h>
#include <malloc.h>
#include <memory.h>

#include "fog_connect.h"

static void fog_on_event(void *pr_connect, short events, void *arg);

static void fc_signal_server_init();

static void fog_usr_data_free(void *arg);

static void *ctx = NULL;


static void fog_on_recv(void *pr_connect, void *arg, void *buf, int size) {
    if (size <= 0) return;
    fog_connection_info *user_data = (fog_connection_info *)arg;
    evbuffer_add(user_data->buff, (char *)buf, size);
    if (user_data->on_recv != NULL) {
        user_data->on_recv(user_data);
    }
}


static void fog_on_close(void *connect, void *arg) {
    fog_connection_info *ud = (fog_connection_info *)arg;
    if (ud->on_close != NULL) {
        ud->on_close(ud);
    }
    fog_usr_data_free(ud);
}


static fog_connection_info *fog_usr_data_new(connect_cb on_connect, receive_cb on_recv,
                                             close_cb on_close) {
    fog_connection_info *ret = (fog_connection_info *)malloc(sizeof(fog_connection_info));

    ret->pr_connect = NULL;
    ret->buff = evbuffer_new();
    ret->on_close = on_close;
    ret->on_connect = on_connect;
    ret->on_recv = on_recv;
    return ret;
}


void fog_set_callbacks(fog_connection_info *ud, connect_cb on_connect, receive_cb on_recv,
                       close_cb on_close) {
    ud->on_close = on_close;
    ud->on_connect = on_connect;
    ud->on_recv = on_recv;
}

static void fog_usr_data_free(void *arg) {
    if (arg == NULL) return;
    fog_connection_info *ud = (fog_connection_info *)arg;
    evbuffer_free(ud->buff);
    free(ud);
}

void fog_exit() {
    if (ctx != NULL) fc_release(ctx);
}


static void fc_signal_server_init() {
    fc_signal_server *signal_info = (fc_signal_server *)malloc(sizeof(fc_signal_server));

    signal_info->ctx = ctx;
    signal_info->url = SIGNAL_SERVER_URL;
    signal_info->path = "/ws";
    signal_info->certificate = NULL;
    signal_info->privatekey = NULL;
    signal_info->port = 7600;


    fc_signal_init(signal_info);
}


static connect_cb g_on_connect = NULL;
static receive_cb g_on_recv = NULL;
static close_cb g_on_close = NULL;


static void fog_on_event(void *pr_connect, short events, void *arg) {
    fog_connection_info *ud = (fog_connection_info *)arg;
    switch (events) {
    case FOG_EVENT_CONNECTED: {
        if (ud != NULL) {
            ud->pr_connect = pr_connect;
        } else if (fc_is_passive(pr_connect)) {
            // this fog node is connected by other fog node
            ud = fog_usr_data_new(g_on_connect, g_on_recv, g_on_close);
            fc_set_userdata(pr_connect, ud);
            ud->pr_connect = pr_connect;
        }
        // set the call back, for handle the msg between the fog nodes
        fc_event_setcb(pr_connect, fog_on_recv, fog_on_close);

        // connect callback
        if (ud->on_connect != NULL) {
            ud->on_connect(ud);
        }
        break;
    }
    case FOG_EVENT_EOF:
    case FOG_EVENT_ERROR:
    case FOG_EVENT_TIMEOUT:
        fc_disconnect(pr_connect);
        break;
    default:
        break;
    }
}


void fog_setup(const char *server_id) {
    ctx = fc_init();
    fc_set_id(server_id);
    fc_passive_link_setcb(ctx, fog_on_event);
    fc_signal_server_init();
}


void fog_service_set_callback(connect_cb on_connect, receive_cb on_recv, close_cb on_close) {
    g_on_connect = on_connect;
    g_on_recv = on_recv;
    g_on_close = on_close;
}


int fog_connect_peer(const char *id, int protocol, connect_cb on_connect, receive_cb on_recv,
                     close_cb on_close) {
    fog_connection_info *ud = fog_usr_data_new(on_connect, on_recv, on_close);
    return fc_connect(ctx, id, protocol, 1, fog_on_event, ud);
}