#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#include <gwindow.h>
#include <glayer.h>
#include <gscreen.h>
#include <gtouch.h>
#include <gfont.h>

LIST_HEAD(_g_window_list_head);

/**
 * 根据窗口id（图层id）查找一个窗口
 */
g_window_t *g_find_window(int win)
{
    g_window_t *gw;
    list_for_each_owner (gw, &_g_window_list_head, wlist) {
        if (gw->layer == win) {
            return gw;
        }
    }
    return NULL;
}

/**
 * 绘制窗口样式
 * @turn: 是否为聚焦状态，1为聚焦，0为失焦
 * @body: 是否重绘窗体背景：1为重绘，0为不重绘
 */
static void __g_paint_window(g_window_t *gw, int turn, int body)
{
    uint32_t back, board, font;
    if (turn) {
        back = GW_ON_BACK_COLOR;
        board = GW_ON_BOARD_COLOR;
        font = GW_ON_FONT_COLOR;
        gw->flags |= GW_FOCUSED;
    } else {
        back = GW_OFF_BACK_COLOR;
        board = GW_OFF_BOARD_COLOR;
        font = GW_OFF_FONT_COLOR;
        gw->flags &= ~GW_FOCUSED;
    }
   
    /* 需要清空位图 */
    g_bitmap_clear(gw->wbmp);
    
    if (body)
        g_rectfill(gw->wbmp, 1, 1, gw->width - 2, gw->height - 2, back);
    
    g_rectfill(gw->wbmp, 1, gw->body_region.top - 1, gw->width - 2, 1, board);

    /* 基础圆角 */
    g_rectfill(gw->wbmp, 2, 0, gw->width - 4, 1, board);
    g_rectfill(gw->wbmp, 0, gw->height - 1, gw->width, 1, board);
    g_rectfill(gw->wbmp, 0, 2, 1, gw->height - 2, board);
    g_rectfill(gw->wbmp, gw->width - 1, 2, 1, gw->height - 2, board);

    uint32_t none_color = GC_ARGB(0, 0, 0, 0);
    /* 左上角 */
    g_putpixel(gw->wbmp, 2, 0, none_color);
    g_putpixel(gw->wbmp, 3, 0, none_color);
    g_putpixel(gw->wbmp, 0, 2, none_color);
    g_putpixel(gw->wbmp, 0, 3, none_color);
    g_putpixel(gw->wbmp, 1, 1, none_color);
    g_putpixel(gw->wbmp, 2, 1, board);
    g_putpixel(gw->wbmp, 1, 2, board);

    /* 右上角 */
    g_putpixel(gw->wbmp, gw->width - 3, 0, none_color);
    g_putpixel(gw->wbmp, gw->width - 4, 0, none_color);
    g_putpixel(gw->wbmp, gw->width - 1, 2, none_color);
    g_putpixel(gw->wbmp, gw->width - 1, 3, none_color);
    g_putpixel(gw->wbmp, gw->width - 2, 1, none_color);
    /* 填充边框 */
    g_putpixel(gw->wbmp, gw->width - 3, 1, board);
    g_putpixel(gw->wbmp, gw->width - 2, 2, board);
    
    g_text(gw->wbmp, gw->width / 2 - (strlen(gw->title) * 
        g_current_font->char_width) / 2, (GW_TITLE_BAR_HIGHT - g_current_font->char_height) / 2,
        gw->title, font);
    
    g_bitmap_sync(gw->wbmp, gw->layer, 0, 0);

    g_set_touch_idel_color_group(&gw->touch_list, board);
    g_paint_touch_group(&gw->touch_list);
    if (body) {
        g_invalid_window(gw->layer);
        g_update_window(gw->layer);
    }
}

static int g_window_close_btn_handler(void *arg)
{
    g_touch_t *tch = (g_touch_t *) arg;
    if (!tch)
        return -1;
    g_window_t *gw = (g_window_t *) tch->extension;

    /* 隐藏窗口后，鼠标位置发生了改变 */
    g_point_t po;
    po.x = -1;
    po.y = -1;
    g_check_touch_state_group(&gw->touch_list, &po);

    return g_post_quit_msg(tch->layer);   /* 邮寄一个消息 */
}

static int g_window_minim_btn_handler(void *arg)
{
    g_touch_t *tch = (g_touch_t *) arg;
    if (!tch)
        return -1;
    g_window_t *gw = (g_window_t *) tch->extension;

    /* 隐藏窗口后，鼠标位置发生了改变 */
    g_point_t po;
    po.x = -1;
    po.y = -1;
    g_check_touch_state_group(&gw->touch_list, &po);

    g_hide_window(tch->layer);
    return 0;
}

static int g_window_maxim_btn_handler(void *arg)
{
    g_touch_t *tch = (g_touch_t *) arg;
    if (!tch)
        return -1;
    g_window_t *gw = (g_window_t *) tch->extension;

    /* 隐藏窗口后，鼠标位置发生了改变 */
    g_point_t po;
    po.x = -1;
    po.y = -1;
    g_check_touch_state_group(&gw->touch_list, &po);

    g_maxim_window(tch->layer);
    return 0;
}

/**
 * 绑定窗口的触碰块，实现关闭、最小化、最大化按钮
 */
int g_window_bind_touch(g_window_t *gw)
{
    g_touch_t *gtc_close;
    g_touch_t *gtc_minim;
    g_touch_t *gtc_maxim;

    gtc_close = g_new_touch(GW_BTN_SIZE, GW_BTN_SIZE);
    if (gtc_close == NULL)
        return -1;

    if (!(gw->flags & GW_NO_MINIM)) {
        gtc_minim = g_new_touch(GW_BTN_SIZE, GW_BTN_SIZE);
        if (gtc_minim == NULL) {
            free(gtc_close);
            return -1;
        }
    }

    if (!(gw->flags & GW_NO_MAXIM)) {
        gtc_maxim = g_new_touch(GW_BTN_SIZE, GW_BTN_SIZE);
        if (gtc_maxim == NULL) {
            if (!(gw->flags & GW_NO_MINIM))
                free(gtc_minim);
            free(gtc_close);
            return -1;
        }
    }

    int x = 16;
    int y = 4;

    g_set_touch_location(gtc_close, x, y);
    g_set_touch_color(gtc_close, GW_OFF_FRONT_COLOR, GC_RED);
    g_set_touch_handler(gtc_close, 0, g_window_close_btn_handler);
    g_set_touch_layer(gtc_close, gw->layer, &gw->touch_list);
    gtc_close->extension = gw;
    
    if (!(gw->flags & GW_NO_MINIM)) {
        x += 16 + 8;
        g_set_touch_location(gtc_minim, x, y);
        g_set_touch_color(gtc_minim, GW_OFF_FRONT_COLOR, GC_YELLOW);
        g_set_touch_handler(gtc_minim, 0, g_window_minim_btn_handler);
        g_set_touch_layer(gtc_minim, gw->layer, &gw->touch_list);
        gtc_minim->extension = gw;
    }

    if (!(gw->flags & GW_NO_MAXIM)) {
        x += 16 + 8;
        g_set_touch_location(gtc_maxim, x, y);
        g_set_touch_color(gtc_maxim, GW_OFF_FRONT_COLOR, GC_GREEN);
        g_set_touch_handler(gtc_maxim, 0, g_window_maxim_btn_handler);
        g_set_touch_layer(gtc_maxim, gw->layer, &gw->touch_list);
        gtc_maxim->extension = gw;
    }

    return 0;
}

/**
 * 创建一个新的窗口，成功返回窗口标识，失败返回-1
 * @width: 窗体宽度
 * @height: 窗体高度
 * 
 */
int g_new_window(char *title, int x, int y, uint32_t width, uint32_t height, uint32_t flags)
{
    if (!title || !width || !height)
        return -1;
    uint32_t lw = width + 2;
    uint32_t lh = height + GW_TITLE_BAR_HIGHT + 1;
    
    int ly = g_new_layer(x, y, lw, lh);
    if (ly < 0) {
        return -1;
    }
    
    /* alloc window */
    g_window_t *gw = malloc(sizeof(g_window_t));
    if (gw == NULL) {
        g_del_layer(ly);
        return -1;
    }
    memset(gw->title, 0, GW_TITLE_LEN);
    strcpy(gw->title, title);
    gw->layer = ly;
    gw->flags = flags;
    gw->x = x;
    gw->y = y;
    gw->width = lw;
    gw->height = lh;
    gw->win_width = width;
    gw->win_height = height;

    gw->wbmp = g_new_bitmap(gw->width, gw->height);
    if (gw->wbmp == NULL) {
        g_del_layer(ly);
        return 0;
    }

    list_add_tail(&gw->wlist, &_g_window_list_head);
    init_list(&gw->touch_list);
    /* 添加触碰块 */
    if (g_window_bind_touch(gw) < 0) {
        g_del_bitmap(gw->wbmp);
        list_del(&gw->wlist);
        g_del_layer(ly);
        return -1;
    }

    g_set_layer_region(ly, LAYER_REGION_DRAG, 0, 0, gw->width, GW_TITLE_BAR_HIGHT);
    
    g_set_layer_flags(ly, LAYER_WINDOW);
    gw->backup.y = gw->backup.x = 0;
    gw->backup.width = gw->backup.height = 0;

    gw->body_region.left = 1;
    gw->body_region.top = GW_TITLE_BAR_HIGHT;
    gw->body_region.right = gw->width - 1;
    gw->body_region.bottom = gw->height - 1;

    g_rect_init(&gw->invalid_rect); /* 初始化脏矩形区域 */

    __g_paint_window(gw, 1, 1); /* 绘制整个窗口 */

    /* 给桌面发送创建窗口消息 */
    int desktop_ly = g_get_layer_desktop(); /* send to desktop */
    if (desktop_ly >= 0) {
        g_msg_t m;
        m.id = GM_WINDOW_CREATE;
        m.target = desktop_ly;
        m.data0 = ly; /* layer id */
        g_send_msg(&m);
    }
    
    if (flags & GW_SHOW)
        g_show_window(gw->layer);

    return ly;
}

/**
 * 允许窗口调整大小
 */
int g_enable_window_resize(int win)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;

    /* enable resize region */
    g_set_layer_region(gw->layer, LAYER_REGION_RESIZE, 4, 4, gw->width-4, gw->height - 4);
    
    gw->flags |= GW_RESIZE;

    return 0;   
}

int g_set_window_title(int win, const char *title)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    
    uint32_t back, board, font;
    if (gw->flags) {
        back = GW_ON_BACK_COLOR;
        board = GW_ON_BOARD_COLOR;
        font = GW_ON_FONT_COLOR;
    } else {
        back = GW_OFF_BACK_COLOR;
        board = GW_OFF_BOARD_COLOR;
        font = GW_OFF_FONT_COLOR;
    }

    int len = strlen(gw->title); // old title
    
    /* 需要清空位图 */
    g_bitmap_clear(gw->wbmp);
    
    // clear old
    g_rectfill(
        gw->wbmp,
        gw->width / 2 - (len *g_current_font->char_width) / 2,
        (GW_TITLE_BAR_HIGHT - g_current_font->char_height) / 2,
        len * g_current_font->char_width,
        g_current_font->char_height,
        back
    );
    // set new title
    memset(gw->title, 0, GW_TITLE_LEN);
    strcpy(gw->title, title);
    
    len = strlen(gw->title);
    // draw new title
    g_text(
        gw->wbmp, gw->width / 2 - (len *g_current_font->char_width) / 2,
        (GW_TITLE_BAR_HIGHT - g_current_font->char_height) / 2,
        gw->title, font
    );

    g_bitmap_sync(gw->wbmp, gw->layer, 0, 0);

    // attention to the buttons
    g_set_touch_idel_color_group(&gw->touch_list, board);
    g_paint_touch_group(&gw->touch_list);

    return 0;   
}

/**
 * 设置最小可调整的窗口大小
 * @min_width: 最小允许调整的宽度
 * @min_height: 最小允许调整的高度
 * 
 */
int g_set_window_minresize(int win, uint32_t min_width, uint32_t min_height)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    if (!min_width || !min_height)
        return -1;
    
    /* set resize min rect, 窗体大小转换成图层大小 */
    g_set_layer_region(gw->layer, LAYER_REGION_RESIZEMIN, 0, 0, min_width + 2,
        min_height + GW_TITLE_BAR_HIGHT + 1);

    return 0;   
}

/**
 * 关闭窗口调整大小功能
 */
int g_disable_window_resize(int win)
{
     g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    /* disable resize region */
    g_set_layer_region(gw->layer, LAYER_REGION_RESIZE, -1, -1, -1, -1);

    gw->flags &= ~GW_RESIZE;
    
    return 0;
}

/**
 * 收到调整窗口大小的消息时需要重新绘制整个窗口
 */
int g_resize_window(int win, uint32_t width, uint32_t height)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    if (!width || !height)
        return -1;
    gw->width = width;
    gw->height = height;

    /* 调整窗口位图大小 */
    g_bitmap_t *bmp = g_new_bitmap(gw->width, gw->height);
    if (bmp == NULL) {
        printf("[gwindow]: %s new bitmap null, abort!\n");
        abort();
    }
    g_del_bitmap(gw->wbmp);
    gw->wbmp = bmp;

    /* 设置拖拽区域 */
    g_set_layer_region(gw->layer, LAYER_REGION_DRAG, 0, 0, width, GW_TITLE_BAR_HIGHT);

    if (gw->flags & GW_RESIZE) { /* 需要有调整大小标志才能进行调整 */
        g_set_layer_region(gw->layer, LAYER_REGION_RESIZE, 4, 4, width-4, height - 4);
    }
    
    gw->win_width = width - 2;
    gw->win_height = height - GW_TITLE_BAR_HIGHT - 1;

    /* 设置窗体区域 */
    gw->body_region.left = 1;
    gw->body_region.top = GW_TITLE_BAR_HIGHT;
    gw->body_region.right = gw->width-1;
    gw->body_region.bottom = gw->height-1;

    __g_paint_window(gw, 1, 1);
    /* TODO: 重绘内容 */

    g_refresh_layer(gw->layer, 0, 0, width, height);
    return 0;
}

/**
 * 聚焦窗口
 * @turn: 聚焦开关，on为聚焦，off为丢焦
 */
int g_focus_window(int win, int turn)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    __g_paint_window(gw, turn, 0);
    g_refresh_layer(gw->layer, 0, 0, gw->width, gw->height);
    /* 发送聚焦/丢焦消息给桌面 */
    int desktop_ly = g_get_layer_desktop(); /* send to desktop */
    if (desktop_ly >= 0) {
        g_msg_t m;
        m.target = desktop_ly;
        m.data0 = gw->layer; /* layer id */
        if (turn) {
            m.id = GM_GET_FOCUS;
        } else {
            m.id = GM_LOST_FOCUS;
        }
        g_send_msg(&m);
    }
    return 0;
}

/**
 * 根据窗口标识（图层id）删除一个窗口。
 * 
 */
int g_del_window(int win)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    
    if (g_del_layer(gw->layer) < 0)
        return -1;
    int desktop_ly = g_get_layer_desktop(); /* send to desktop */
    if (desktop_ly >= 0) {
        /* 给桌面发送关闭窗口消息 */
        g_msg_t m;
        m.id = GM_WINDOW_CLOSE;
        m.target = desktop_ly;
        m.data0 = gw->layer; /* layer id */
        g_send_msg(&m);
    }
    g_del_touch_group(&gw->touch_list);
    list_del(&gw->wlist);
    g_del_bitmap(gw->wbmp);
    free(gw);
    g_focus_layer_win_top(); /* 删除后需要聚焦顶层窗口 */
    return 0;
}

int g_del_window_all()
{
    g_window_t *gw, *next;
    list_for_each_owner_safe (gw, next, &_g_window_list_head, wlist) {
        if (g_del_window(gw->layer) < 0)
            return -1;
    }
    return 0;
}

/**
 * 窗口第一次显示在桌面上，从隐藏变成显示状态
 */
int g_show_window(int win)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    int wintop = g_get_layer_wintop();
    g_set_layer_z(gw->layer, wintop);
    g_focus_layer_win_top(); /* 显示后需要聚焦顶层窗口 */
    /* 给桌面发送关闭窗口消息 */
    int desktop_ly = g_get_layer_desktop(); /* send to desktop */
    if (desktop_ly >= 0) {
        g_msg_t m;
        m.id = GM_SHOW;
        m.target = desktop_ly;
        m.data0 = gw->layer; /* layer id */
        g_send_msg(&m);
    }
    return 0;
}

/**
 * 隐藏指定窗口，从桌面消失，变成隐藏状态
*/
int g_hide_window(int win)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    
    g_set_layer_z(gw->layer, -1);
    g_focus_layer_win_top(); /* 隐藏后需要聚焦顶层窗口 */
    int desktop_ly = g_get_layer_desktop(); /* send to desktop */
    if (desktop_ly >= 0) {
        /* 给桌面发送关闭窗口消息 */
        g_msg_t m;
        m.id = GM_HIDE;
        m.target = desktop_ly;
        m.data0 = gw->layer; /* layer id */
        g_send_msg(&m);
    }
    return 0;
}

/**
 * 窗口执行最大化操作。
 * 如果不是最大化，那么进行最大化。
 * 如果已经是最大化，那么就恢复之前窗口的大小和位置。
 */
int g_maxim_window(int win)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    g_rect_t rect;
    if (gw->flags & GW_MAXIM) {
        /* 处于最大化状态，恢复原来窗口大小 */
        rect.x = gw->backup.x;
        rect.y = gw->backup.y;
        rect.width = gw->backup.width;
        rect.height = gw->backup.height;
        gw->flags &= ~GW_MAXIM;  /* 取消最大化状态 */

        /* 恢复原来的窗体信息 */    
        gw->body_region.left = 1;
        gw->body_region.top = GW_TITLE_BAR_HIGHT;
        gw->body_region.right = gw->width-1;
        gw->body_region.bottom = gw->height-1;
        
        if (gw->flags & GW_RESIZE_EX) {
            /* 以前有调整大小，恢复调整大小 */
            g_enable_window_resize(gw->layer);
            gw->flags &= ~GW_RESIZE_EX; /* 解封调整大小 */
        }
    } else {    /* 进行最大化 */
        gw->backup.x = gw->x;
        gw->backup.y = gw->y;
        gw->backup.width = gw->width;
        gw->backup.height = gw->height;
        
        rect.x = g_screen.window_region.left;
        rect.y = g_screen.window_region.top;
        rect.width = g_screen.window_region.right;
        rect.height = g_screen.window_region.bottom - g_screen.window_region.top;
        gw->flags |= GW_MAXIM;  /* 最大化状态 */
    
        /* 设置窗体区域为整个图层 */    
        gw->body_region.left = 0;
        gw->body_region.top = GW_TITLE_BAR_HIGHT;
        gw->body_region.right = gw->width;
        gw->body_region.bottom = gw->height; 

        if (gw->flags & GW_RESIZE) {
            /* 禁止调整大小 */
            g_disable_window_resize(gw->layer);
            /* 禁止调整大小，但恢复后需要保留原来的状态，曾经有调整大小标志，只是暂时被封印了 */
            gw->flags |= GW_RESIZE_EX;
        }
    }

    if (g_resize_layer(gw->layer, rect.x, rect.y, rect.width, rect.height) < 0)
        return -1;

    if (g_focus_layer(gw->layer) < 0) {
        /* TODO: 恢复原来的大小 */
        return -1;
    }
    return 0;
}

int g_paint_window(int win, int x, int y, g_bitmap_t *bmp)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    g_rect_t rect;
    rect.x = x + gw->body_region.left;
    rect.y = y + gw->body_region.top;
    rect.width = bmp->width;
    rect.height = bmp->height;
    return g_sync_layer_bitmap(
        gw->layer,
        &rect,
        bmp->buffer,
        &gw->body_region);
}

int g_paint_window_ex(int win, int x, int y, g_bitmap_t *bmp)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    g_rect_t rect;
    rect.x = x + gw->body_region.left;
    rect.y = y + gw->body_region.top;
    rect.width = bmp->width;
    rect.height = bmp->height;
    return g_sync_layer_bitmap_ex(
        gw->layer,
        &rect,
        bmp->buffer,
        &gw->body_region);
}

/**
 * g_paint_window_copy - 从窗口中复制绘制内容到bmp中
 * 
 */
int g_paint_window_copy(int win, int x, int y, g_bitmap_t *bmp)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    g_rect_t rect;
    rect.x = x + gw->body_region.left;
    rect.y = y + gw->body_region.top;
    rect.width = bmp->width;
    rect.height = bmp->height;
    return g_copy_layer_bitmap(
        gw->layer,
        &rect,
        bmp->buffer,
        &gw->body_region);
}

/**
 * 刷新窗口矩形区域
 * 
 */
int g_refresh_window_rect(int win, int x, int y, uint32_t width, uint32_t height)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    
    g_refresh_layer_rect(win, gw->body_region.left + x, gw->body_region.top + y, width, height);
    return 0;
}

/**
 * 刷新窗口矩形区域
 * 
 */
int g_refresh_window_region(int win, int left, int top, int right, int bottom)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    
    g_refresh_layer(win, gw->body_region.left + left, gw->body_region.top + top,
        gw->body_region.left + right, gw->body_region.top + bottom);
    return 0;
}

/**
 * 检测脏矩形，如果有就发送GM_PAINT消息
 */
int g_update_window(int win)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    if (g_rect_valid(&gw->invalid_rect)) { /* 脏矩形有效 */
        /* 发送GM_PAINT重绘消息 */
        g_msg_t m;
        m.id = GM_PAINT;
        m.target = gw->layer;
        g_send_msg(&m);
    }
    return 0;
}

/**
 * 设置脏矩形，部分
 * 
 */
int g_invalid_rect(int win, int x, int y, uint32_t width, uint32_t height)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    if (g_rect_valid(&gw->invalid_rect)) { /* 脏矩形有效 */
        /* 合并脏区域 */
        g_rect_t rect = {x, y, width, height};
        g_rect_merge(&gw->invalid_rect, &rect);
    } else {
        /* 直接设置脏矩形 */
        g_rect_set(&gw->invalid_rect, x, y, width, height);
    }
    return 0;
}

/**
 * 设置脏矩形，整个窗口
 */
int g_invalid_window(int win)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    g_rect_set(&gw->invalid_rect, 0, 0, gw->win_width, gw->win_height);
    return 0;  
}

/**
 * 获取脏矩形，这个必须是在GM_PAINT中调用
 */
int g_get_invalid(int win, int *x, int *y, uint32_t *width, uint32_t *height)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    if (x)
        *x = gw->invalid_rect.x;
    if (y)
        *y = gw->invalid_rect.y;
    if (width)
        *width = gw->invalid_rect.width;
    if (height)
        *height = gw->invalid_rect.height;
    
    g_rect_init(&gw->invalid_rect);
    return 0;
}

/**
 * 隐藏指定窗口，从桌面消失，变成隐藏状态
*/
int g_set_window_icon(int win, char *path)
{
    g_window_t *gw = g_find_window(win);
    if (!gw)
        return -1;
    
    /* 设置图标缓冲区 */
    g_set_icon_path(gw->layer, path);
    
    int desktop_ly = g_get_layer_desktop(); /* send to desktop */
    if (desktop_ly >= 0) {
        /* 给桌面发送关闭窗口消息 */
        g_msg_t m;
        m.id = GM_WINDOW_ICON;
        m.target = desktop_ly;
        m.data0 = gw->layer; /* layer id */
        g_send_msg(&m);
    }

    return 0;
}
