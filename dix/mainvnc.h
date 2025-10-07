#ifndef VNC_MAIN_H
#define VNC_MAIN_H


#include "inputstr.h"
#include "vnc_libs/img.h"
#include "pixmap.h"   
#include "damage.h"
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#include "vnc_libs/vncqueue.h"
#include "vnc_libs/websocket.h"
#include "vnc_libs/vnc_input.h"

#define getDelay(fps) (1000000 / (fps))

int httpPort = 0;
char config[128]; // make sure this buffer is large enough]
DamageQueue * gdq;
int appRunning = 0;
ScreenPtr g_screen;
pthread_t ws_thread;
pthread_t vnc_thread;
int currentScreen = 0;
Websocket * g_ws = NULL;
int isServerRunning = 0;
static DamagePtr myDamage = NULL;
unsigned char * buffer;
extern Bool EnableCursor;

void VNC_init(void);
void VNC_loop(void);
void VNC_close(void);
void VNC_cleanup(void);
void ws_onconnect(int sid);
void VNC_log(const char *msg);
void* ws_thread_func(void* arg);
void* vnc_thread_func(void* arg);
void sendFrame(void);
void initMyDamage(ScreenPtr pScreen);
void VNC_service_init(ScreenPtr screen);
void VNC_log_appended_int(const char * msg,int val);
void vncSendFrame(int x, int y, int width, int height);
static void DamageExtDestroy(DamagePtr pDamage, void *closure){}
void myDamageReport(DamagePtr pDamage, RegionPtr pRegion, void *closure);


void VNC_init(void) {
    EnableCursor = FALSE;
    gdq = malloc(sizeof(DamageQueue));
    if (!gdq) { perror("malloc"); return; }
    dq_init(gdq, 0, 0, 100);
    buffer = (unsigned char *)malloc(20000000 * sizeof(unsigned char));
    appRunning = 1;
    VNC_loop();
}

void VNC_service_init(ScreenPtr screen) {
    VNC_log("XwebVNC service started");
    VNC_log("XwebVNC server started: success stage 1");    
    VNC_log_appended_int("[XwebVNC] > INF: XwebVNC server listening on port %d\n",httpPort);
    VNC_log("> (help: use -web or -http to change, e.g. -web 8000 or -http 8000)"); 
    initMyDamage(screen);
    input_init(&inputInfo, g_ws);
    
    dq_reset(gdq, screen->width, screen->height);
    isServerRunning = 1;
    g_screen = screen;
    snprintf(config, sizeof(config), "{'screen':%d,'width':%d,'height':%d, 'quality':%d}", screen->myNum, screen->width, screen->height, outputQuality);
    VNC_log(config);
    VNC_log("XwebVNC server started: success stage 2");
}

void myDamageReport(DamagePtr pDamage, RegionPtr pRegion, void *closure) {
    int nboxes;
    pixman_box16_t *boxes = pixman_region_rectangles(pRegion, &nboxes);
    for (int i = 0; i < nboxes && appRunning; i++) {
        Rect r;
        r.x1 = boxes[i].x1;
        r.y1 = boxes[i].y1;
        r.x2 = boxes[i].x2;
        r.y2 = boxes[i].y2;
        dq_push(gdq, r);
    }
}

void initMyDamage(ScreenPtr pScreen) {
    myDamage = DamageCreate(myDamageReport,
                            DamageExtDestroy,
                            DamageReportRawRegion,
                            FALSE,
                            pScreen,
                            NULL);
    if (!myDamage) {
        printf("Failed to create Damage object\n");
        return;
    }
    // Attach to root window
    DamageRegister((DrawablePtr)pScreen->root, myDamage);
    VNC_log_appended_int("[XwebVNC] > LOG: Damage listener attached to screen %d root window\n", pScreen->myNum);
}


void VNC_log(const char *msg) {
    printf("[XwebVNC] > LOG: %s\n", msg);
}
 
void VNC_log_appended_int(const char * msg,int val) {
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-nonliteral"
    printf(msg, val);
    #pragma GCC diagnostic pop
}

void VNC_cleanup(void) {
    VNC_log("Session on-close activity: starting");
    isServerRunning = 0;
}

void* ws_thread_func(void* arg) {
    free(arg);
    arg = NULL;
    ws_connections(g_ws); // Run your poll/select loop
    return NULL;
}

void* vnc_thread_func(void* arg) {
    int delay = getDelay(11);
    while(appRunning) {
        sendFrame();
        usleep(delay);
        // process_key_press(38, 1);
        // process_key_press(38, 0);
    }
    printf("vnc thread end\n");
    return NULL;
}

void VNC_loop(void) {
    g_ws = malloc(sizeof(Websocket));
    if (!g_ws) { perror("malloc"); return; }

    ws_init(g_ws);
    ws_begin(g_ws, httpPort);
    g_ws->callBack = ws_onconnect;
    g_ws->callBackMsg = process_client_Input;

    if (pthread_create(&ws_thread, NULL, ws_thread_func, 0) != 0) {
        perror("Failed to create websocket thread");
        return;
    }

    if (pthread_create(&vnc_thread, NULL, vnc_thread_func, 0) != 0) {
        perror("Failed to create VMC thread");
        return;
    }
}

void sendFrame(void) {
    if(!isServerRunning) return;
    int i = dq_merge(gdq);
    while(isServerRunning && i--){
        Rect r;
        if(!dq_get(gdq, &r)) return;
        if(g_ws->clients < 1) return;
        vncSendFrame(r.x1, r.y1, r.x2 - r.x1, r.y2 - r.y1);
    }
}

void ws_onconnect(int sid) {
    VNC_log_appended_int("[XwebVNC] > INF: New Websocket client connection established! sid: %d\n", sid);
    snprintf(config, sizeof(config), "{'screen':%d,'width':%d,'height':%d, 'quality':%d}", g_screen->myNum, g_screen->width, g_screen->height, outputQuality);
    ws_sendText(g_ws, config, sid);
    vncSendFrame(0, 0, g_screen->width, g_screen->height);
}

void vncSendFrame(int x, int y, int width, int height) {
    if(!isServerRunning) return;
    if(force_full_screen_refresh){
        x = 0;
        y = 0;
        width = g_screen->width;
        height = g_screen->height;
        force_full_screen_refresh = 0;
        snprintf(config, sizeof(config), "{'screen':%d,'width':%d,'height':%d, 'quality':%d}", g_screen->myNum, g_screen->width, g_screen->height, outputQuality);
        ws_sendText(g_ws, config, -1);
    }
    int stride;
    extractRectRGB(g_screen, x, y, width, height, &stride);
    int img_size = 0;
    PixmapPtr pPix = (*g_screen->GetScreenPixmap)(g_screen);
    unsigned char *rect_ptr = (unsigned char *)pPix->devPrivate.ptr + y * pPix->devKind + x * 4;
    char *jpeg_data = (char* )compress_image_to_jpeg(rect_ptr, stride, width, height, &img_size, outputQuality);
    char data[256];  // adjust size if needed
    snprintf(
        data, sizeof(data),
        "VPD%d %d %d %d %d %d \n", x, y, width, height, 24, img_size
    );
    ws_p_sendRaw(g_ws, 130, data, jpeg_data, strlen(data), img_size, -1);
    free(jpeg_data);
}

void VNC_close(void) {
    VNC_cleanup();
    VNC_log("XServer on-close activity: starting");

    // 1. Destroy and free damage queue
    if (gdq) {
        dq_destroy(gdq);
        free(gdq);
        gdq = NULL;
    }

    // 2. Free capture buffer
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }

    // 4. Stop app + server flags
    isServerRunning = 0;
    appRunning = 0;
    // 5. Stop websocket server
    if (g_ws) {
        g_ws->stop = 1;

        if (g_ws->server_fd > 0) {
            close(g_ws->server_fd);
            g_ws->server_fd = -1;
        }
    }

    VNC_log("Session closed: success");

    // 6. Join threads (wait for them to exit cleanly)
    pthread_join(vnc_thread, NULL);
    pthread_detach(ws_thread);
    // 7. Free websocket struct
    if (g_ws) {
        free(g_ws);
        g_ws = NULL;
    }
    sleep(2);
}


#endif