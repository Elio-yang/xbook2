#include <string.h>
#include <stdio.h>
#include <xbook/driver.h>
#include <sys/ioctl.h>
#include <xbook/kmalloc.h>
#include <xbook/vmarea.h>

/// 程序本地头文件
#include <gui/screen.h>

#define DEBUG_GUI_SCREEN

#ifndef   GUI_SCREEN_DEVICE_NAME
#define   GUI_SCREEN_DEVICE_NAME        "video"
#endif

gui_screen_t gui_screen = {0};

static  video_info_t    video_info;
static  int             video_handle           = -1;
static  unsigned int    video_ram_size  = 0;
static unsigned char  *video_ram_start = NULL;
static  unsigned int    bits_per_pixel  = 0;
static  unsigned int    bytes_per_pixel = 0;
static  unsigned int    screen_width    = 0;
static  unsigned int    screen_height   = 0;

static int screen_open(void)
{
    video_handle = device_open( GUI_SCREEN_DEVICE_NAME, 0 );
    if ( video_handle < 0 ) 
        return  -1;

    video_ram_size = video_info.bytes_per_scan_line * video_info.y_resolution;
#ifdef DEBUG_GUI_SCREEN
    printf("[gui]: screen memory map size %x\n", video_ram_size);
#endif
    video_ram_start = device_mmap(video_handle, video_ram_size, IO_KERNEL);
    if (video_ram_start == NULL) {
        printf("[gui]: video mapped failed!\n");
        return -1;
    }
#ifdef DEBUG_GUI_SCREEN
    printf("[gui]: mapped addr %x\n", video_ram_start);
#endif
    gui_screen.vram_start = video_ram_start;
    return  0;
}

static int screen_close(void)
{
    iounmap(video_ram_start);
    device_close(video_handle);
    video_ram_start = NULL;
    video_handle = -1;
    return  1;
}

static  int  screen_output_pixel8(int x, int y, GUI_COLOR  color)
{
    uint32_t  r, g, b;

    b = color&0xC0;
    g = color&0xE000;
    r = color&0xE00000;
    /* User must write the right code */
	/*int  offset = 0;
    offset = (screen_width)*y+x;*/
    *(video_ram_start + (screen_width)*y+x) = (unsigned char)((b>>6)|(g>>11)|(r>>16));
    return  0;
}

static  int  screen_output_pixel15(int x, int y, GUI_COLOR  color)
{
    uint32_t  r, g, b;
    b = color&0xF8;
    g = color&0xF800;
    r = color&0xF80000;
    /* User must write the right code */
	/*int  offset = 0;
    offset = 2*((screen_width)*y+x);*/
    *((short int*)((video_ram_start) + 2*((screen_width)*y+x))) = (short int)((b>>3)|(g>>6)|(r>>9));
    return  0;
}

static  int  screen_output_pixel16(int x, int y, GUI_COLOR  color)
{
    uint8_t  r, g, b;
    b = color&0xF8;
    g = color&0xFC00;
    r = color&0xF80000;
    /* User must write the right code */
	/*int  offset = 0;
    offset = 2*((screen_width)*y+x);*/
    *((short int*)((video_ram_start) + 2*((screen_width)*y+x))) = (short int)((b>>3)|(g>>5)|(r>>8));
    return  0;
}

static  int  screen_output_pixel24(int x, int y, GUI_COLOR  color)
{
    /* User must write the right code */
	/*int  offset = 0;
    offset = 3*((screen_width)*y+x);*/
    *((video_ram_start) + 3*((screen_width)*y+x) + 0) = color&0xFF;
    *((video_ram_start) + 3*((screen_width)*y+x) + 1) = (color&0xFF00) >> 8;
    *((video_ram_start) + 3*((screen_width)*y+x) + 2) = (color&0xFF0000) >> 16;
    return  0;
}

static  int  screen_output_pixel32(int x, int y, GUI_COLOR  color)
{
    /* User must write the right code */
	/*int  offset = 0;
    offset = 4*((screen_width)*y+x);*/
    *((int*)((video_ram_start) + 4*((screen_width)*y+x))) = (int)color;
    return  0;
}

static  int  screen_input_pixel8(int x, int y, SCREEN_COLOR *color)
{
    /* Maybe have code or maybe have not code */
    int           offset    = 0;
    offset = (screen_width)*y+x;
    *color = *(video_ram_start+offset);
    
    return  0;
}

static  int  screen_input_pixel15(int x, int y, SCREEN_COLOR *color)
{
    /* Maybe have code or maybe have not code */
    int           offset    = 0;
    offset = 2*((screen_width)*y+x);
    *color = *((short int*)(video_ram_start + offset));
    return  0;
}

static  int  screen_input_pixel16(int x, int y, SCREEN_COLOR *color)
{
    /* Maybe have code or maybe have not code */
    int           offset    = 0;
    offset = 2*((screen_width)*y+x);
    *color = *((short int*)(video_ram_start + offset));
    return  0;
}

static  int  screen_input_pixel24(int x, int y, SCREEN_COLOR *color)
{
    /* Maybe have code or maybe have not code */
    int           offset    = 0;
    SCREEN_COLOR  tmp_color = 0;

    offset = 3*((screen_width)*y+x);
    tmp_color  = *(video_ram_start + offset + 0)&0xFF;
    tmp_color |= ((*(video_ram_start + offset + 1))&0xFF00) >> 8;
    tmp_color |= ((*(video_ram_start + offset + 2))&0xFF0000) >> 16;
    *color     = tmp_color;
    return  0;
}

static  int  screen_input_pixel32(int x, int y, SCREEN_COLOR *color)
{
    /* Maybe have code or maybe have not code */
    int           offset    = 0;
    offset = 4*((screen_width)*y+x);
    *color = *((int*)(video_ram_start + offset));
    return  0;
}

static  int  screen_detect_var(gui_screen_t *screen)
{
    int ret = 0;

    video_handle = device_open( GUI_SCREEN_DEVICE_NAME, 0 );
    if ( video_handle < 0 ) 
        return  -1;

    ret = device_devctl(video_handle, VIDEOIO_GETINFO, (unsigned long) &video_info);
    if ( ret < 0 ) 
        return  -1;

    screen->width     = video_info.x_resolution;
    screen->height    = video_info.y_resolution;
    
    bits_per_pixel   = video_info.bits_per_pixel;
    bytes_per_pixel  = (video_info.bits_per_pixel+7)/8;
    screen->bpp = bits_per_pixel;
    
    screen_width     = screen->width;
    screen_height    = screen->height;
#ifdef DEBUG_GUI_SCREEN
    printf("video info: w:%d h:%d bpp:%d \n", screen->width, screen->height, video_info.bits_per_pixel);
#endif
    switch (video_info.bits_per_pixel) 
    {
    case 8:
        screen->output_pixel = screen_output_pixel8;
        screen->input_pixel  = screen_input_pixel8;
        break;
    case 15:
        screen->output_pixel = screen_output_pixel15;
        screen->input_pixel  = screen_input_pixel15;

        break;

    case 16:
        screen->output_pixel = screen_output_pixel16;
        screen->input_pixel  = screen_input_pixel16;

        break;

    case 24:
        screen->output_pixel = screen_output_pixel24;
        screen->input_pixel  = screen_input_pixel24;

        break;

    case 32:
        screen->output_pixel = screen_output_pixel32;
        screen->input_pixel  = screen_input_pixel32;
        break;

    default:
        break;
    }

    device_close(video_handle);
    video_handle = -1;
    return  0;
}

/* Hardware accelerbrate interface */
static  int  screen_output_hline(int left, int right, int top, SCREEN_COLOR  color)
{
    SCREEN_COLOR  a_color;
    SCREEN_COLOR  b_color;
    SCREEN_COLOR  c_color;

    int  offset = 0;
    int  i      = 0;

    if ( left > (screen_width-1) )
        return  -1;
    if ( right > (screen_width-1) )
        return  -1;

    if ( top > (screen_height-1) )
        return  -1;

    switch( (bits_per_pixel) )
    {
        case  8:
            offset = (screen_width)*top+left;
            if ( offset >= ((screen_width)*(screen_height)-1))
                return  -1;

            for ( i = 0; i <= right - left; i++ )
    	        *(video_ram_start+offset+i) = (unsigned char)color;

            break;

        case  15:
            offset = 2*((screen_width)*top+left);
            if ( offset >= ((screen_width)*(screen_height)*2-2))
                return  -1;

            for ( i = 0; i <= right - left; i++ )
    	        *((short int*)((video_ram_start) + offset + 2*i)) = (short int)color;

            break;

        case  16:
            offset = 2*((screen_width)*top+left);
            if ( offset >= ((screen_width)*(screen_height)*2-2))
                return  -1;

            for ( i = 0; i <= right - left; i++ )
    	        *((short int*)((video_ram_start) + offset + 2*i)) = (short int)color;

            break;

        case  24:
            offset = 3*((screen_width)*top+left);
            if ( offset >= ((screen_width)*(screen_height)*3-3))
                return  -1;

            a_color = color&0xFF;
    	    b_color = (color&0xFF00) >> 8;
    	    c_color = (color&0xFF0000) >> 16;
            for ( i = 0; i <= right - left; i++ )
            {
    	        *((video_ram_start) + offset + 3*i + 0) = a_color;
    	        *((video_ram_start) + offset + 3*i + 1) = b_color;
    	        *((video_ram_start) + offset + 3*i + 2) = c_color;
            }
            break;

        case  32:
            offset = 4*((screen_width)*top+left);
            if ( offset >= ((screen_width)*(screen_height)*4-4))
                return  -1;

            for ( i = 0; i <= right - left; i++ )
    	        *((int*)((video_ram_start) + offset + 4*i)) = (int)color;

            break;

        default:
            break;

    }

    return  0;
}

/* Hardware accelerbrate interface */
static  int  screen_output_vline(int left, int top, int bottom, SCREEN_COLOR  color)
{
    SCREEN_COLOR  a_color;
    SCREEN_COLOR  b_color;
    SCREEN_COLOR  c_color;

    int  offset = 0;
    int  i      = 0;

    if ( left > (screen_width-1) )
        return  -1;
    if ( top > (screen_height-1) )
        return  -1;
    if ( bottom > (screen_height-1) )
        return  -1;


    switch( (bits_per_pixel) )
    {
        case  8:
            for ( i = 0; i <= bottom - top; i++ )
            {
                offset = (screen_width)*(top+i)+left;
                if ( offset >= ((screen_width)*(screen_height)-1))
                    return  -1;

    	        *(video_ram_start+offset) = (unsigned char)color;
            }
            break;

        case  15:
            for ( i = 0; i <= bottom - top; i++ )
            {
                offset = 2*((screen_width)*(top+i)+left);
                if ( offset >= ((screen_width)*(screen_height)*2-2))
                    return  -1;

    	        *((short int*)((video_ram_start) + offset)) = (short int)color;
            }
            break;

        case  16:
            for ( i = 0; i <= bottom - top; i++ )
            {
                offset = 2*((screen_width)*(top+i)+left);
                if ( offset >= ((screen_width)*(screen_height)*2-2))
                    return  -1;

    	        *((short int*)((video_ram_start) + offset)) = (short int)color;
            }
            break;

        case  24:
            a_color = color&0xFF;
    	    b_color = (color&0xFF00) >> 8;
    	    c_color = (color&0xFF0000) >> 16;

            for ( i = 0; i <= bottom - top; i++ )
            {
                offset = 3*((screen_width)*(top+i)+ left);
                if ( offset >= ((screen_width)*(screen_height)*3-3))
                    return  -1;

       	        *((video_ram_start) + offset + 0) = a_color;
    	        *((video_ram_start) + offset + 1) = b_color;
    	        *((video_ram_start) + offset + 2) = c_color;
            }
            break;

        case  32:
            for ( i = 0; i <= bottom - top; i++ )
            {
                offset = 4*((screen_width)*(top+i)+left);
                if ( offset >= ((screen_width)*(screen_height)*4-4))
                    return  -1;

    	        *((int*)((video_ram_start) + offset)) = (int)color;
            }
            break;

        default:
            break;

    }

    return  0;
}

/* Hardware accelerbrate interface */
static  int  screen_output_rect_fill(int left, int top, int right, int bottom, SCREEN_COLOR  color)
{
    SCREEN_COLOR  a_color;
    SCREEN_COLOR  b_color;
    SCREEN_COLOR  c_color;

    int  offset = 0;
    int  i      = 0;
    int  j      = 0;

    if ( left > (screen_width-1) )
        return  -1;
    if ( right > (screen_width-1) )
        right = (screen_width-1);

    if ( top > (screen_height-1) )
        return  -1;

    switch( (bits_per_pixel) )
    {
        case  8:
            for ( j = 0; j <= bottom - top; j++ )
            {
                offset = (screen_width)*(top+j)+left;
                if ( offset >= ((screen_width)*(screen_height)-1))
                    return  -1;

                for ( i = 0; i <= right - left; i++ )
    	            *(video_ram_start+offset+i) = (unsigned char)color;
            }
            break;

        case  15:
            for ( j = 0; j <= bottom - top; j++ )
            {
                offset = 2*((screen_width)*(top+j)+left);
                if ( offset >= ((screen_width)*(screen_height)*2-2))
                    return  -1;

                for ( i = 0; i <= right - left; i++ )
    	            *((short int*)((video_ram_start) + offset + 2*i)) = (short int)color;
            }
            break;

        case  16:
            for ( j = 0; j <= bottom - top; j++ )
            {
                offset = 2*((screen_width)*(top+j)+left);
                if ( offset >= ((screen_width)*(screen_height)*2-2))
                    return  -1;

                for ( i = 0; i <= right - left; i++ )
    	            *((short int*)((video_ram_start) + offset + 2*i)) = (short int)color;
            }
            break;

        case  24:
            a_color = color&0xFF;
    	    b_color = (color&0xFF00) >> 8;
    	    c_color = (color&0xFF0000) >> 16;
            for ( j = 0; j <= bottom - top; j++ )
            {
                offset = 3*((screen_width)*(top+j)+left);
                if ( offset >= ((screen_width)*(screen_height)*3-3))
                    return  -1;

                for ( i = 0; i <= right - left; i++ )
                {
    	            *((video_ram_start) + offset + 3*i + 0) = a_color;
    	            *((video_ram_start) + offset + 3*i + 1) = b_color;
    	            *((video_ram_start) + offset + 3*i + 2) = c_color;
                }
            }
            break;

        case  32:
            for ( j = 0; j <= bottom - top; j++ )
            {
                offset = 4*((screen_width)*(top+j)+left);
                if ( offset >= ((screen_width)*(screen_height)*4-4))
                    return  -1;

                for ( i = 0; i <= right - left; i++ )
    	            *((int*)((video_ram_start) + offset + 4*i)) = (int)color;
            }
            break;

        default:
            break;

    }
    return  1;
}

int sys_screen_get(g_screen_t *screen)
{
    if (screen) {
        *screen = gui_screen.screen;
        return 0;
    }
    return -1;
}

int sys_screen_set_window_region(gui_region_t *region)
{
    if (region) {
        gui_screen.screen.window_region = *region;
        return 0;
    }
    return -1;
}

int gui_init_screen()
{
    int ret = 0;

    memset(&gui_screen, 0, sizeof(gui_screen));
    ret = screen_detect_var(&gui_screen);
    if (ret < 0)
        return -1;
    
    gui_screen.open = screen_open;
    gui_screen.close = screen_close;

    gui_screen.output_vline = screen_output_vline;
    gui_screen.output_hline = screen_output_hline;
    gui_screen.output_rect_fill = screen_output_rect_fill;
    
    /* 初始化屏幕信息 */
    gui_screen.screen.width = gui_screen.width;
    gui_screen.screen.height = gui_screen.height;
    gui_screen.screen.bits_per_pixel = gui_screen.bpp;
    gui_region_init(&gui_screen.screen.window_region);

    return 0;
}
