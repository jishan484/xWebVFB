#include "os/osdep.h"
#include "xkbsrv.h"
#include "xwebvnc/webvnc.h"
#include "scrnintstr.h"
#include "windowstr.h"
#include <jpeglib.h>


void XWEBVNC_init_input(void) {
    DeviceIntPtr dev;
    for (dev = inputInfo.devices; dev; dev = dev->next) {
        if (dev->type == MASTER_KEYBOARD) {
            XkbDescPtr xkb = inputInfo.keyboard->key->xkbInfo->desc;
            if (!xkb)
                return;
            int min = xkb->min_key_code;
            int max = xkb->max_key_code;

            for (int kc = min; kc <= max; ++kc) {
                XkbSymMapPtr map = &xkb->map->key_sym_map[kc];

                for (int level = 0; level < map->width; ++level) {
                    KeySym ks = xkb->map->syms[map->offset + level];
                    if (ks != NoSymbol) {
                        add_mapping(ks, kc);
                    }
                }
            }
        }
    }
}

void screen_pix_config(ScreenPtr pScreen, XScreenConf * screenConf) {
    PixmapPtr pPix              = (*pScreen->GetScreenPixmap)(pScreen);
    screenConf->buffer_ptr      = pPix->devPrivate.ptr;
    screenConf->stride          = pPix->devKind;       // bytes per row
    screenConf->bit_per_pixel   = pPix->drawable.bitsPerPixel;
}

unsigned char *extractRectRGB16or32(ScreenPtr pScreen,
                                int x, int y,
                                int rect_w, int rect_h,
                                XScreenConf * screenConf)
{
    return (screenConf->bit_per_pixel == 32) ? (unsigned char *)(screenConf->buffer_ptr + y * screenConf->stride + x * 4)
    : (unsigned char *)(screenConf->buffer_ptr + y * screenConf->stride + x * 2);
}

unsigned char *compress_image_to_jpeg(unsigned char *fb_data,
                                        int stride,
                                        int width, int height,
                                        int *out_size,
                                        int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    unsigned char *jpeg_data = NULL;
    unsigned long jpeg_size = 0;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    jpeg_mem_dest(&cinfo, &jpeg_data, &jpeg_size);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 4;     // 4 bytes per pixel (B,G,R,X)
    cinfo.in_color_space = JCS_EXT_BGRX;  // <-- ignore alpha

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        unsigned char *row_pointer =
            fb_data + cinfo.next_scanline * stride;
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    *out_size = (int)jpeg_size;
    jpeg_destroy_compress(&cinfo);

    return jpeg_data;  // caller must free()
}

void getSubImage(int x, int y, int rect_w, int rect_h, XScreenConf *screenConf, char *temp_buffer) {
    if (!screenConf || !screenConf->buffer_ptr || !temp_buffer) return;

    int bytes_per_pixel = screenConf->bit_per_pixel / 8;
    int row_bytes = rect_w * bytes_per_pixel;  // number of bytes to copy per row
    unsigned char *src = screenConf->buffer_ptr + y * screenConf->stride + x * bytes_per_pixel;
    unsigned char *dst = (unsigned char*)temp_buffer;

    for (int i = 0; i < rect_h; i++) {
        memcpy(dst, src, row_bytes);  // copy only rectangle width, not full stride
        dst += row_bytes;             // move output pointer
        src += screenConf->stride;    // move source pointer to next row in framebuffer
    }
}


Atom XWEBVNC_get_pointer_sprite_name(void) {
    return inputInfo.pointer->spriteInfo->sprite->current->name;
}