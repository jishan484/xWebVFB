#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ----------------- TYPES -----------------
typedef struct {
    int x1, y1, x2, y2;
} Rect;

typedef struct {
    Rect *rects;        // ring buffer of rects
    int head;           // dequeue index
    int tail;           // enqueue index
    int count;          // number of rects
    int max_sub_frame;  // capacity

    Rect *scratch;      // pre-allocated scratch buffer for merging
} RectQueue;

typedef struct {
    int max_screen_width;
    int max_screen_height;
    RectQueue queue;
    pthread_mutex_t mtx;
} DamageQueue;

// ----------------- UTILITIES -----------------
static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline int max_int(int a, int b) { return a > b ? a : b; }

static inline bool rects_intersect(const Rect *a, const Rect *b) {
    return !(a->x2 <= b->x1 || b->x2 <= a->x1 ||
             a->y2 <= b->y1 || b->y2 <= a->y1);
}

// ----------------- INIT / DESTROY -----------------
void dq_init(DamageQueue *dq, int screen_w, int screen_h, int max_sub);
void dq_init(DamageQueue *dq, int screen_w, int screen_h, int max_sub) {
    dq->max_screen_width  = screen_w;
    dq->max_screen_height = screen_h;

    dq->queue.max_sub_frame = max_sub > 0 ? max_sub : 100;
    dq->queue.rects   = (Rect *) malloc(sizeof(Rect) * dq->queue.max_sub_frame);
    dq->queue.scratch = (Rect *) malloc(sizeof(Rect) * dq->queue.max_sub_frame);
    dq->queue.head    = 0;
    dq->queue.tail    = 0;
    dq->queue.count   = 0;

    pthread_mutex_init(&dq->mtx, NULL);
}

void dq_reset(DamageQueue *dq, int screen_w, int screen_h);
void dq_reset(DamageQueue *dq, int screen_w, int screen_h) {
    dq->max_screen_width  = screen_w;
    dq->max_screen_height = screen_h;

    dq->queue.head    = 0;
    dq->queue.tail    = 0;
    dq->queue.count   = 0;
}

void dq_destroy(DamageQueue *dq);
void dq_destroy(DamageQueue *dq) {
    free(dq->queue.rects);
    free(dq->queue.scratch);
    pthread_mutex_destroy(&dq->mtx);
}

// ----------------- METHODS -----------------

// Push a rect into queue (overflow → full screen rect)
void dq_push(DamageQueue *dq, Rect r);
void dq_push(DamageQueue *dq, Rect r) {
    // pthread_mutex_lock(&dq->mtx);
    // pthread_cleanup_push((void(*)(void*))pthread_mutex_unlock, (void*)&dq->mtx);

    if (dq->queue.count < dq->queue.max_sub_frame) {
        dq->queue.rects[dq->queue.tail] = r;
        dq->queue.tail = (dq->queue.tail + 1) % dq->queue.max_sub_frame;
        dq->queue.count++;
    } else {
        dq->queue.head  = 0;
        dq->queue.tail  = 1;
        dq->queue.count = 1;
        dq->queue.rects[0].x1 = 0;
        dq->queue.rects[0].y1 = 0;
        dq->queue.rects[0].x2 = dq->max_screen_width;
        dq->queue.rects[0].y2 = dq->max_screen_height;
    }

    // pthread_cleanup_pop(1); // always unlock
}

// Merge overlapping rects (optimized, in-place)
int dq_merge(DamageQueue *dq);
int dq_merge(DamageQueue *dq) {
    pthread_mutex_lock(&dq->mtx);

    int n = dq->queue.count;
    if (n <= 1) {
        pthread_mutex_unlock(&dq->mtx);
        return n;
    }

    // Copy current queue into scratch buffer in linear order
    for (int i = 0, idx = dq->queue.head; i < n; i++) {
        dq->queue.scratch[i] = dq->queue.rects[idx];
        idx = (idx + 1) % dq->queue.max_sub_frame;
    }

    // Merge in scratch
    int outCount = 0;
    for (int i = 0; i < n; i++) {
        Rect cur = dq->queue.scratch[i];
        int merged = 1;
        while (merged) {
            merged = 0;
            for (int j = i + 1; j < n; j++) {
                if (rects_intersect(&cur, &dq->queue.scratch[j])) {
                    cur.x1 = min_int(cur.x1, dq->queue.scratch[j].x1);
                    cur.y1 = min_int(cur.y1, dq->queue.scratch[j].y1);
                    cur.x2 = max_int(cur.x2, dq->queue.scratch[j].x2);
                    cur.y2 = max_int(cur.y2, dq->queue.scratch[j].y2);
                    dq->queue.scratch[j] = dq->queue.scratch[--n];
                    merged = 1;
                    break;
                }
            }
        }
        dq->queue.scratch[outCount++] = cur;
    }

    // Write back merged rects into ring buffer
    dq->queue.head  = 0;
    dq->queue.tail  = outCount;
    dq->queue.count = outCount;
    for (int i = 0; i < outCount; i++) {
        dq->queue.rects[i] = dq->queue.scratch[i];
    }

    pthread_mutex_unlock(&dq->mtx);
    return outCount;
}

bool dq_hasNext(DamageQueue *dq);
bool dq_hasNext(DamageQueue *dq) {
    return dq->queue.count > 0;
}

bool dq_get(DamageQueue *dq, Rect *out);
bool dq_get(DamageQueue *dq, Rect *out) {
    pthread_mutex_lock(&dq->mtx);
    if (dq->queue.count == 0) {
        pthread_mutex_unlock(&dq->mtx);
        return false;
    }
    *out = dq->queue.rects[dq->queue.head];
    dq->queue.head = (dq->queue.head + 1) % dq->queue.max_sub_frame;
    dq->queue.count--;
    pthread_mutex_unlock(&dq->mtx);
    return true;
}