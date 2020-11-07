#include <xbook/syscall.h>
#include <xbook/process.h>
#include <xbook/vmspace.h>
#include <xbook/resource.h>
#include <xbook/trigger.h>
#include <xbook/alarm.h>
#include <xbook/ktime.h>
#include <xbook/clock.h>
#include <xbook/waitque.h>
#include <xbook/srvcall.h>
#include <xbook/fs.h>
#include <xbook/driver.h>
#include <xbook/net.h>
#include <xbook/sharemem.h>
#include <xbook/sem.h>
#include <xbook/msgqueue.h>
#include <xbook/gui.h>
#include <sys/stat.h>
#include <dirent.h>

#include <gui/layer.h>
#include <gui/message.h>
#include <gui/screen.h>
#include <gui/timer.h>

/* 系统调用表 */ 
syscall_t syscall_table[SYSCALL_NR];

void init_syscall()
{
    /* 进程管理 */
    syscall_table[SYS_EXIT] = sys_exit;
    syscall_table[SYS_FORK] = sys_fork;
    syscall_table[SYS_WAITPID] = sys_waitpid;
    syscall_table[SYS_GETPID] = sys_get_pid;
    syscall_table[SYS_GETPPID] = sys_get_ppid;
    
    syscall_table[SYS_TRIGGER] = sys_trigger_handler;
    syscall_table[SYS_TRIGGERON] = sys_trigger_active;
    syscall_table[SYS_TRIGGERACT] = sys_trigger_action;
    syscall_table[SYS_TRIGRET] = sys_trigger_return;
    
    syscall_table[SYS_SLEEP] = sys_sleep;
    
    syscall_table[SYS_THREAD_CREATE] = sys_thread_create;
    syscall_table[SYS_THREAD_EXIT] = sys_thread_exit;
    syscall_table[SYS_THREAD_JOIN] = sys_thread_join;
    syscall_table[SYS_THREAD_DETACH] = sys_thread_detach;
    
    syscall_table[SYS_GETTID] = sys_get_tid;
    
    syscall_table[SYS_THREAD_CANCEL] = sys_thread_cancel;
    syscall_table[SYS_THREAD_TESTCANCEL] = sys_thread_testcancel;
    syscall_table[SYS_THREAD_CANCELSTATE] = sys_thread_setcancelstate;
    syscall_table[SYS_THREAD_CANCELTYPE] = sys_thread_setcanceltype;
    
    syscall_table[SYS_SCHED_YEILD] = sys_sched_yeild;
    
    syscall_table[SYS_WAITQUE_CREATE] = sys_waitque_create;
    syscall_table[SYS_WAITQUE_DESTROY] = sys_waitque_destroy;
    syscall_table[SYS_WAITQUE_WAIT] = sys_waitque_wait;
    syscall_table[SYS_WAITQUE_WAKE] = sys_waitque_wake;

    /* 内存管理 */
    syscall_table[SYS_HEAP] = sys_vmspace_heap;
    syscall_table[SYS_MUNMAP] = sys_munmap;
    
    syscall_table[SYS_ALARM] = sys_alarm;
    syscall_table[SYS_KTIME] = sys_get_ktime;
    syscall_table[SYS_GETTICKS] = sys_get_ticks;
    syscall_table[SYS_GETTIMEOFDAY] = sys_gettimeofday;
    syscall_table[SYS_CLOCK_GETTIME] = sys_clock_gettime;
    
    syscall_table[SYS_SRVCALL] = sys_srvcall;
    syscall_table[SYS_SRVCALL_LISTEN] = sys_srvcall_listen;
    syscall_table[SYS_SRVCALL_ACK] = sys_srvcall_ack;
    syscall_table[SYS_SRVCALL_BIND] = sys_srvcall_bind;
    syscall_table[SYS_SRVCALL_UNBIND] = sys_srvcall_unbind;
    syscall_table[SYS_SRVCALL_FETCH] = sys_srvcall_fetch;
    
    syscall_table[SYS_UNID] = sys_unid;
    syscall_table[SYS_TSTATE] = sys_tstate;
    syscall_table[SYS_GETVER] = sys_getver;
    syscall_table[SYS_MSTATE] = sys_mstate;    
    syscall_table[SYS_USLEEP] = sys_usleep;    
    /* 文件系统 */
    syscall_table[SYS_OPEN] = sys_open;
    syscall_table[SYS_CLOSE] = sys_close;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_LSEEK] = sys_lseek;
    syscall_table[SYS_ACCESS] = sys_access;
    syscall_table[SYS_UNLINK] = sys_unlink;
    syscall_table[SYS_FTRUNCATE] = sys_ftruncate;
    syscall_table[SYS_FSYNC] = sys_fsync;
    syscall_table[SYS_IOCTL] = sys_ioctl;
    syscall_table[SYS_FCNTL] = sys_fcntl;
    syscall_table[SYS_TELL] = sys_tell;
    syscall_table[SYS_MKDIR] = sys_mkdir;
    syscall_table[SYS_RMDIR] = sys_rmdir;
    syscall_table[SYS_RENAME] = sys_rename;
    syscall_table[SYS_CHDIR] = sys_chdir;
    syscall_table[SYS_GETCWD] = sys_getcwd;
    syscall_table[SYS_EXECVE] = sys_execve;
    syscall_table[SYS_STAT] = sys_stat;
    syscall_table[SYS_FSTAT] = sys_fstat;
    syscall_table[SYS_CHMOD] = sys_chmod;
    syscall_table[SYS_FCHMOD] = sys_fchmod;
    syscall_table[SYS_OPENDIR] = sys_opendir;
    syscall_table[SYS_CLOSEDIR] = sys_closedir;
    syscall_table[SYS_READDIR] = sys_readdir;
    syscall_table[SYS_REWINDDIR] = sys_rewinddir;
    syscall_table[SYS_MKFS] = sys_mkfs;
    syscall_table[SYS_MOUNT] = sys_mount;
    syscall_table[SYS_UNMOUNT] = sys_unmount;

    /* socket 套接字 */
    syscall_table[SYS_SOCKET] = sys_socket;
    syscall_table[SYS_BIND] = sys_bind;
    syscall_table[SYS_CONNECT] = sys_connect;
    syscall_table[SYS_LISTEN] = sys_listen;
    syscall_table[SYS_ACCEPT] = sys_accept;
    syscall_table[SYS_SEND] = sys_send;
    syscall_table[SYS_RECV] = sys_recv;
    syscall_table[SYS_SENDTO] = sys_sendto;
    syscall_table[SYS_RECVFROM] = sys_recvfrom;
    syscall_table[SYS_SHUTDOWN] = sys_shutdown;
    syscall_table[SYS_GETPEERNAME] = sys_getpeername;
    syscall_table[SYS_GETSOCKNAME] = sys_getsockname;
    syscall_table[SYS_GETSOCKOPT] = sys_getsockopt;
    syscall_table[SYS_SETSOCKOPT] = sys_setsockopt;
    syscall_table[SYS_IOCTLSOCKET] = sys_ioctlsocket;
    syscall_table[SYS_SELECT] = sys_select;
    syscall_table[SYS_DUP] = sys_dup;
    syscall_table[SYS_DUP2] = sys_dup2;
    syscall_table[SYS_PIPE] = sys_pipe;
    
    syscall_table[SYS_SHMGET] = sys_shmem_get;
    syscall_table[SYS_SHMPUT] = sys_shmem_put;
    syscall_table[SYS_SHMMAP] = sys_shmem_map;
    syscall_table[SYS_SHMUNMAP] = sys_shmem_unmap;
    syscall_table[SYS_SEMGET] = sys_sem_get;
    syscall_table[SYS_SEMPUT] = sys_sem_put;
    syscall_table[SYS_SEMDOWN] = sys_sem_down;
    syscall_table[SYS_SEMUP] = sys_sem_up;
    syscall_table[SYS_MSGGET] = sys_msgque_get;
    syscall_table[SYS_MSGPUT] = sys_msgque_put;
    syscall_table[SYS_MSGSEND] = sys_msgque_send;
    syscall_table[SYS_MSGRECV] = sys_msgque_recv;
     
    syscall_table[SYS_TRIGPENDING] = sys_trigger_pending;
    syscall_table[SYS_TRIGPROCMASK] = sys_trigger_proc_mask;

    /* gui */
    syscall_table[SYS_LAYERNEW] = sys_new_layer;
    syscall_table[SYS_LAYERDEL] = sys_del_layer;
    syscall_table[SYS_LAYERZ] = sys_layer_z;
    syscall_table[SYS_LAYERMOVE] = sys_layer_move;
    syscall_table[SYS_LAYERREFRESH] = sys_layer_refresh;
    syscall_table[SYS_LAYERGETWINTOP] = sys_layer_get_win_top;
    syscall_table[SYS_LAYERSETWINTOP] = sys_layer_set_win_top;
    syscall_table[SYS_GINIT] = sys_g_init;
    syscall_table[SYS_GQUIT] = sys_g_quit;
    syscall_table[SYS_GGETMSG] = sys_g_get_msg;
    syscall_table[SYS_GTRYGETMSG] = sys_g_try_get_msg;
    syscall_table[SYS_LAYERSETFOCUS] = sys_layer_set_focus;
    syscall_table[SYS_LAYERGETFOCUS] = sys_layer_get_focus;
    syscall_table[SYS_LAYERSETREGION] = sys_layer_set_region;
    syscall_table[SYS_GPOSTMSG] = sys_g_post_msg;
    syscall_table[SYS_GSENDMSG] = sys_g_send_msg;
    syscall_table[SYS_LAYERSETFLG] = sys_layer_set_flags;
    syscall_table[SYS_LAYERRESIZE] = sys_layer_resize;
    syscall_table[SYS_LAYERFOCUS] = sys_layer_focus;
    syscall_table[SYS_LAYERFOCUSWINTOP] = sys_layer_focus_win_top;
    syscall_table[SYS_GSCREENGET] = sys_screen_get;
    syscall_table[SYS_GSCREENSETWINRG] = sys_screen_set_window_region;
    syscall_table[SYS_LAYERGETDESKTOP] = sys_layer_get_desktop;
    syscall_table[SYS_LAYERSETDESKTOP] = sys_layer_set_desktop;
    syscall_table[SYS_GNEWTIMER] = sys_gui_new_timer;
    syscall_table[SYS_GMODIFYTIMER] = sys_gui_modify_timer;
    syscall_table[SYS_GDELTIMER] = sys_gui_del_timer;
    syscall_table[SYS_LAYERSYNCBMP] = sys_layer_sync_bitmap;
    syscall_table[SYS_LAYERSYNCBMPEX] = sys_layer_sync_bitmap_ex;
    syscall_table[SYS_LAYERCOPYBMP] = sys_layer_copy_bitmap;
    syscall_table[SYS_GGETICONPATH] = sys_gui_get_icon;
    syscall_table[SYS_GSETICONPATH] = sys_gui_set_icon;
    syscall_table[SYS_PROBE] = sys_probe;
}
