#include <stdio.h>
#include <gapi.h>
#include <unistd.h>

int win_proc(g_msg_t *msg);

int rate_progress = 0;
int fps = 0;
#define RATE_BAR_LEN    360

#define WIDTH   320
#define HEIGHT  240

void timer_handler(int layer, uint32_t msgid, uint32_t tmrid, uint32_t time)
{
    g_set_timer(layer, tmrid, 50, timer_handler);
    
    char buf[32] = {0};
    sprintf(buf, "[win]: fps=%d", fps);
    g_set_window_title(layer, buf);
    printf("%s!\n", buf);
    fps = 0;
}

g_bitmap_t *screen_bitmap;
int main(int argc, char *argv[]) 
{
    /* 初始化 */
    if (g_init() < 0)
        return -1;

    int win = g_new_window("win", 0, 0, WIDTH, HEIGHT, GW_SHOW);
    if (win < 0)
        return -1;
        
    g_enable_window_resize(win);
    g_set_window_minresize(win, 200, 100);
    /* 设置窗口界面 */
    
    g_set_timer(win, 0x1000, 1000, timer_handler);

    g_msg_t msg;
    int msg_ret;
    while (1)
    {
        /* 获取消息，无消息返回0，退出消息返回-1，有消息返回1 */
        msg_ret = g_try_get_msg(&msg);
        if (msg_ret < 0)
            break;
        if (!msg_ret)
            continue;
        /* 有外部消息则处理消息 */
        win_proc(&msg);
    }

    g_quit();

    return 0;
}
int win_proc(g_msg_t *msg)
{
    int x, y;
    uint32_t w, h;
    int keycode, keymod;
    int win;
    static uint32_t color = GC_RED;
    
    switch (g_msg_get_type(msg))
    {
    case GM_KEY_DOWN:
        keycode = g_msg_get_key_code(msg);
        keymod = g_msg_get_key_modify(msg);
        
        printf("[win]: down keycode=%d:%c modify=%x\n", keycode, keycode, keymod);
        g_translate_msg(msg);
        keycode = g_msg_get_key_code(msg);
        keymod = g_msg_get_key_modify(msg);
        printf("[win]: down keycode=%d:%c modify=%x\n", keycode, keycode, keymod);
        
        if (keycode == GK_SPACE) {
            printf("[win]: down space\n");
        }
        break;
    case GM_KEY_UP:
        keycode = g_msg_get_key_code(msg);
        keymod = g_msg_get_key_modify(msg);
        printf("[win]: up keycode=%d:%c modify=%x\n", keycode, keycode, keymod);
        g_translate_msg(msg);
        keycode = g_msg_get_key_code(msg);
        keymod = g_msg_get_key_modify(msg);
        printf("[win]: up keycode=%d:%c modify=%x\n", keycode, keycode, keymod);
        if (keycode == GK_SPACE) {
            printf("[win]: up space\n");
        }
        break;
    case GM_MOUSE_MOTION:
    case GM_MOUSE_LBTN_DOWN:
    case GM_MOUSE_LBTN_UP:
    case GM_MOUSE_LBTN_DBLCLK:
    case GM_MOUSE_RBTN_DOWN:
    case GM_MOUSE_RBTN_UP:
    case GM_MOUSE_RBTN_DBLCLK:
    case GM_MOUSE_MBTN_DOWN:
    case GM_MOUSE_MBTN_UP:
    case GM_MOUSE_MBTN_DBLCLK:
    case GM_MOUSE_WHEEL_UP:
    case GM_MOUSE_WHEEL_DOWN:
        x = g_msg_get_mouse_x(msg);
        y = g_msg_get_mouse_y(msg);
        //printf("[win]: mouse %d, %d\n", x, y);
        break;
    case GM_PAINT:
        win = g_msg_get_target(msg);
        g_get_invalid(win, &x, &y, &w, &h);
        
        screen_bitmap = g_new_bitmap(w, h);
        if (screen_bitmap == NULL)
            break;
        g_rectfill(screen_bitmap, 0, 0, w, h, color);
        g_paint_window(win, 0, 0, screen_bitmap);
        //g_refresh_window_rect(win, 0, 0, w, h);
        
        g_del_bitmap(screen_bitmap);
        screen_bitmap = NULL;
        
        g_invalid_window(win);
        g_update_window(win);
        color += 0x05040302;
        fps++;
        break;  
    default:
        break;
    }
    return 0;
}
