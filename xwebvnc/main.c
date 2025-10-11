#include "input_priv.h"
#include "scrnintstr.h"
#include "damage.h"
#include "webvnc.h"
#include <unistd.h>
#include <lz4.h>


#define getDelay(fps) (995000 / (fps)) //fixed drift applied


static char *app_buffer;
int app_running_indicator;
static pthread_t ws_thread;
static int app_buffer_size;
static pthread_t vnc_thread;
struct XScreenConf screenConf;
static char vnc_App_config[128];
static DamagePtr myDamage = NULL;
static Websocket *global_websocket;
static char * app_temp_frame_buffer;
static ScreenPtr global_screen_pointer;
static DamageQueue *global_damage_queue;

void ws_onconnect(int sid);
void *ws_thread_func(void *arg);
void *vnc_thread_func(void *arg);
void XWEBVNC_send_next_frame(void);
void XWEBVNC_damage_setup(ScreenPtr screen);
static void DamageExtDestroy(DamagePtr pDamage, void *closure){}
void myDamageReport(DamagePtr pDamage, RegionPtr pRegion, void *closure);



/*
*  vnc setup method, call once the main method's cli arguments are parsed
-> takes nothing
-> returns nothing
*/
void XWEBVNC_setup(void) {
  XWEBVNC_log("Initializing App");

  EnableCursor = FALSE;
  global_damage_queue = dq_init(0, 0, 100);
  app_buffer_size = 1920 * 1080 * 4 * sizeof(unsigned char);
  app_buffer = (char *)malloc(app_buffer_size);
  app_temp_frame_buffer = (char *)malloc(app_buffer_size);
  global_websocket = ws_init();

  if (!global_damage_queue || !app_buffer || !global_websocket) {
    XWEBVNC_log("Initialization failed! Excluding XWebVNC extension");
    if (global_damage_queue)
      free(global_damage_queue);
    if (global_websocket)
      free(global_websocket);
    if (app_buffer)
      free(app_buffer);
    return;
  }

  app_running_indicator = 1;
  app_output_quality = 20;

  XWEBVNC_start_app_main_loop();
}


/*
*  vnc init method, call every time the main method's while loop iterates
-> takes screen ponter
-> returns nothing
*/
void XWEBVNC_init(ScreenPtr screen) {
    global_screen_pointer = screen;

    XWEBVNC_damage_setup(screen);
    dq_reset(global_damage_queue, screen->width, screen->height);
    screen_pix_config(screen, &screenConf);
    snprintf(vnc_App_config, sizeof(vnc_App_config), "{'screen':%d,'width':%d,'height':%d, 'quality':%d, 'depth':%d}", screen->myNum, screen->width, screen->height, app_output_quality, screenConf.bit_per_pixel);
    XWEBVNC_log(vnc_App_config);
    input_init(global_websocket);
}

/*
*  vnc cleanup method, call every time the main method's while loop iterates
-> takes nothing
-> returns nothing
-> does nothing ( required for future plans )
*/
void XWEBVNC_cleanup(void) {
  // nothing to do for now
  XWEBVNC_log("X Server Session restarted");
}

/*
vnc close method, call at the end of main method
-> takes nothing
-> returns nothing
*/
void XWEBVNC_close(void) {
    XWEBVNC_log("XServer on-close cleanup activity started");

    app_running_indicator = 0;

    ws_close(global_websocket);
    pthread_join(vnc_thread, NULL);
    pthread_join(ws_thread, NULL);

    if (global_damage_queue) {
        dq_destroy(global_damage_queue);
        free(global_damage_queue);
        global_damage_queue = NULL;
    }

    if (app_buffer) {
        free(app_buffer);
        app_buffer = NULL;
    }

    if (app_temp_frame_buffer) {
        free(app_temp_frame_buffer);
        app_temp_frame_buffer = NULL;
    }

    if(global_websocket) {
        free(global_websocket);
        global_websocket = NULL;
    }

    XWEBVNC_log("Session closed successfully");
}

void XWEBVNC_start_app_main_loop(void) {
  ws_begin(global_websocket, XWEBVNC_http_server_port);
  ws_assign(global_websocket, process_client_Input, ws_onconnect);

  if (pthread_create(&ws_thread, NULL, ws_thread_func, 0) != 0) {
    perror("Failed to create websocket thread");
    return;
  }

  if (pthread_create(&vnc_thread, NULL, vnc_thread_func, 0) != 0) {
    perror("Failed to create VMC thread");
    return;
  }
}

/*
vnc send frame method, call to send frame of size x,y,w,h
-> takes size of frame
-> must be valid size (no check has been added in this)
-> returns nothing
*/
void XWEBVNC_send_frame(int x, int y, int width, int height) {
  if (force_full_screen_refresh) {
    x = 0;
    y = 0;
    width = global_screen_pointer->width;
    height = global_screen_pointer->height;
    force_full_screen_refresh = 0;

    snprintf(vnc_App_config, sizeof(vnc_App_config),
             "{'screen':%d,'width':%d,'height':%d, 'quality':%d, 'depth':%d}",
             global_screen_pointer->myNum, global_screen_pointer->width,
             global_screen_pointer->height, app_output_quality, screenConf.bit_per_pixel);
    ws_sendText(global_websocket, vnc_App_config, -1);
  }
  
  int img_size = 0;
  char data[256];
  
  if(app_output_quality < 100) {
        unsigned char *screen_image_rect_ptr = extractRectRGB16or32(global_screen_pointer, x, y, width, height, &screenConf);
        char *jpeg_data = (char* )compress_image_to_jpeg(screen_image_rect_ptr, screenConf.stride, width, height, &img_size, app_output_quality);

        snprintf(
            data, sizeof(data),
            "VPD%d %d %d %d %d %d \n", x, y, width, height, 24, img_size
        );
        ws_p_sendRaw(global_websocket, 130, data, jpeg_data, strlen(data), img_size, -1);
        free(jpeg_data);
  } else {
        int bytes_per_pixel = screenConf.bit_per_pixel / 8;
        int frameSize = height * width * bytes_per_pixel;

        getSubImage(x, y, width, height, &screenConf, app_temp_frame_buffer);
        img_size = LZ4_compress_default((const char *)app_temp_frame_buffer,
                                        app_buffer, frameSize, app_buffer_size);
        snprintf(data, sizeof(data), "UPD%d %d %d %d %d %d \n", x, y, width, height,
                width * bytes_per_pixel, img_size);
        ws_p_sendRaw(global_websocket, 130, data, app_buffer, strlen(data),
                    img_size, -1);
  }
}




  //------------------//
 // internal methods //
//------------------//

void ws_onconnect(int sid) {
  XWEBVNC_log_append("New Websocket client connection established! sid: ", sid);
  if (!global_screen_pointer) return;
  ws_sendText(global_websocket, vnc_App_config, sid);
  XWEBVNC_send_frame(0, 0, global_screen_pointer->width,
                     global_screen_pointer->height);
}

void XWEBVNC_send_next_frame(void) {
  if (!app_running_indicator) return;
  int i = dq_merge(global_damage_queue);
  while (app_running_indicator && ws_has_client(global_websocket) && i--) {
    Rect r;
    if (!dq_get(global_damage_queue, &r)) return;
    XWEBVNC_send_frame(r.x1, r.y1, r.x2 - r.x1, r.y2 - r.y1);
  }
}

void *ws_thread_func(void *arg) {
  free(arg);
  arg = NULL;
  ws_connections(global_websocket);
  XWEBVNC_log("http/ws server thread ended");
  return NULL;
}

void *vnc_thread_func(void *arg) {
  int delay = getDelay(15);
  while (app_running_indicator) {
    XWEBVNC_send_next_frame();
    usleep(delay);
  }
  XWEBVNC_log("vnc main thread ended");
  return NULL;
}

void XWEBVNC_damage_setup(ScreenPtr screen) {
  myDamage = DamageCreate(myDamageReport,
                            DamageExtDestroy,
                            DamageReportRawRegion,
                            FALSE,
                            screen,
                            NULL);
    if (!myDamage) {
        XWEBVNC_log("Failed to create Damage object\n");
        return;
    }
    // : Attach to root window is required
    DamageRegister((DrawablePtr)screen->root, myDamage);
    ErrorF("[XwebVNC] > LOG: Damage listener attached to screen %d root window\n", screen->myNum);
}

void myDamageReport(DamagePtr pDamage, RegionPtr pRegion, void *closure) {
    int nboxes;
    pixman_box16_t *boxes = pixman_region_rectangles(pRegion, &nboxes);
    if(!app_running_indicator && !global_damage_queue) return;
    for (int i = 0; i < nboxes; i++) {
        Rect r;
        r.x1 = boxes[i].x1;
        r.y1 = boxes[i].y1;
        r.x2 = boxes[i].x2;
        r.y2 = boxes[i].y2;
        dq_push(global_damage_queue, r);
    }
}

//------------------//
// logs for XWebVNC //
//------------------//
void XWEBVNC_log(const char *message) { ErrorF("[XwebVNC] > %s\n", message); }
void XWEBVNC_log_append(const char *message, int number) {
  ErrorF("[XwebVNC] > %s%d\n", message, number);
}