#include "scrnintstr.h"
#include "pixmapstr.h"
#include <jpeglib.h>


unsigned char *extractRectRGB(ScreenPtr pScreen,
                                int x, int y,
                                int rect_w, int rect_h,
                                int *stride_out);
unsigned char *extractRectRGB(ScreenPtr pScreen,
                                int x, int y,
                                int rect_w, int rect_h,
                                int *stride_out)
{
    PixmapPtr pPix = (*pScreen->GetScreenPixmap)(pScreen);
    char *fb_base  = (char *) pPix->devPrivate.ptr;
    int stride     = pPix->devKind;       // bytes per row
    int bpp        = pPix->drawable.bitsPerPixel;

    if (bpp != 32) {
        fprintf(stderr, "Only 32 bpp supported here (got %d)\n", bpp);
        return NULL;
    }

    // Clamp rect
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + rect_w > pPix->drawable.width)
        rect_w = pPix->drawable.width - x;
    if (y + rect_h > pPix->drawable.height)
        rect_h = pPix->drawable.height - y;

    *stride_out = stride;
    return (unsigned char *)(fb_base + y * stride + x * (bpp / 8));
}


unsigned char *compress_image_to_jpeg(unsigned char *fb_data,
                                        int stride,
                                        int width, int height,
                                        int *out_size,
                                        int quality);
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
