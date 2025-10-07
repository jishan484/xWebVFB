#ifndef WEBVNC
#define WEBVNC
#include "screenint.h"
#define MAX_CLIENTS 30


#include <stdbool.h>

// variables
typedef struct Rect Rect;
typedef struct XScreenConf XScreenConf;
extern int app_output_quality;
extern int app_running_indicator;
typedef struct Websocket Websocket;
extern int XWEBVNC_http_server_port;
extern int force_full_screen_refresh;
typedef struct DamageQueue DamageQueue;


  //------------------------------------------------//
 //                  Main App                      //
//------------------------------------------------//

void XWEBVNC_init(ScreenPtr screen);
void XWEBVNC_close(void);
void XWEBVNC_setup(void);
void XWEBVNC_cleanup(void);
void XWEBVNC_log(const char * message);
void XWEBVNC_start_app_main_loop(void);
void XWEBVNC_send_frame(int x, int y, int w, int h);
void XWEBVNC_log_append(const char * message, int number);


  //------------------------------------------------//
 //                  VNC Queue                     //
//------------------------------------------------//

//
int  dq_merge(DamageQueue *dq);
bool dq_hasNext(DamageQueue *dq);
void dq_destroy(DamageQueue *dq);
void dq_push(DamageQueue *dq, Rect r);
bool dq_get(DamageQueue *dq, Rect *out);
void dq_reset(DamageQueue *dq, int screen_w, int screen_h);
DamageQueue * dq_init(int screen_w, int screen_h, int max_sub);


  //------------------------------------------------//
 //                 VNC WebSocket                  //
//------------------------------------------------//

//
Websocket * ws_init(void);
void ws_close(Websocket *ws);
int ws_has_client(Websocket *ws);
void ws_connections(Websocket *ws);
void ws_begin(Websocket *ws, int port);
void ws_decode(unsigned char *src, char *dest);
void ws_sendText(Websocket *ws, char *text, int sid);
void ws_sendFrame(Websocket *ws, char *img, long size, int sid);
void ws_onMessage(Websocket *ws, void (*ptr)(char *data, int sid));
void ws_handshake(Websocket *ws, unsigned char *data, int sd, int sid);
void ws_sendRaw(Websocket *ws, int startByte, char *data, long size, int sid);
void ws_assign(Websocket *ws, void (*a)(char * message, int i), void (*b)(int j));
void ws_p_sendRaw(Websocket *ws, int startByte, char *data1, char *data2, long data1Size, long data2Size, int sid);


  //------------------------------------------------//
 //              VNC Input Processor               //
//------------------------------------------------//

//
void XWEBVNC_init_input(void);
int  lookup_keycode(KeySym sym);
void input_init(Websocket * gws);
void process_mouse_move(int x, int y);
void add_mapping(long sym, int keycode);
void process_mouse_scroll(int direction);
Atom XWEBVNC_get_pointer_sprite_name(void);
void process_key_press(int keycode, int is_pressed);
void process_client_Input(char *data, int clientSD);
int  buildstr(char *buff, const char *prefix, int val);
void process_mouse_drag(int x1, int y1, int x2, int y2);


  //------------------------------------------------//
 //              VNC Image Processor               //
//------------------------------------------------//

//
unsigned char *extractRectRGB16or32(ScreenPtr pScreen,
                                int x, int y,
                                int rect_w, int rect_h,
                                XScreenConf * screenConf);
unsigned char *compress_image_to_jpeg(unsigned char *fb_data,
                                        int stride,
                                        int width, int height,
                                        int *out_size,
                                        int quality);
void screen_pix_config(ScreenPtr pScreen, XScreenConf * screenConf);
void getSubImage( int x, int y,int rect_w, int rect_h, XScreenConf * screenConf, char * temp_buffer);



  //------------------------------------------------//
 //               VNC Type Definitions             //
//------------------------------------------------//
struct Rect{
    int x1, y1, x2, y2;
} ;

typedef struct {
    Rect *rects;        // ring buffer of rects
    int head;           // dequeue index
    int tail;           // enqueue index
    int count;          // number of rects
    int max_sub_frame;  // capacity

    Rect *scratch;      // pre-allocated scratch buffer for merging
} RectQueue;

struct DamageQueue{
    int max_screen_width;
    int max_screen_height;
    RectQueue queue;
    pthread_mutex_t mtx;
} ;

struct Websocket {
    int server_fd;
    int client_socket[MAX_CLIENTS];
    int ws_client_socket[MAX_CLIENTS];
    int clients;
    int socketPort;
    bool stop;
    int ready;

    // Callbacks
    void (*callBack)(int sid);
    void (*callBackMsg)(char *data, int sid);
};

struct XScreenConf {
  int stride;
  int bit_per_pixel;
  unsigned char * buffer_ptr;
};


#endif