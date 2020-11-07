#include <fsal/fsal.h>
#include <fsal/fatfs.h>
#include <fsal/dir.h>

#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <stdio.h>

#include <xbook/kmalloc.h>
#include <xbook/debug.h>
#include <xbook/fs.h>
#include <xbook/schedule.h>

// #define DEBUG_FSAL


/* 文件表指针 */
fsal_file_t *fsal_file_table;

/**
 * init_fsal_file_table - 初始化文件表
 * 
 * 分配文件表内存，并清空
 */
int init_fsal_file_table()
{
    fsal_file_table = kmalloc(FSAL_FILE_OPEN_NR * sizeof(fsal_file_t));
    if (fsal_file_table == NULL) 
        return -1;
    memset(fsal_file_table, 0, FSAL_FILE_OPEN_NR * sizeof(fsal_file_t));
    return 0;
}

/**
 * fsal_file_alloc - 从表中分配一个文件
 * 
 */
fsal_file_t *fsal_file_alloc()
{
    int i;
    for (i = 0; i < FSAL_FILE_OPEN_NR; i++) {
        if (!fsal_file_table[i].flags) {
            /* 清空文件表的内容 */
            memset(&fsal_file_table[i], 0, sizeof(fsal_file_t));
            
            /* 记录使用标志 */
            fsal_file_table[i].flags = FSAL_FILE_USED;

            return &fsal_file_table[i];
        }
    }
    return NULL;
}

/**
 * fsal_file_free - 释放一个文件
 * 
 */
int fsal_file_free(fsal_file_t *file)
{
    if (!file->flags)
        return -1;
    file->flags = 0;
    return 0;
}

int fsal_list_dir(char* path)
{
    dirent_t de;
    int i;
    int dir = fsif.opendir(path);
    if (dir >= 0) {
        //printk("opendir %s ok.\n", path);
        while (1) {
            /* 读取目录项 */
            if (fsif.readdir(dir, &de) < 0)
                break;
            
            if (de.d_attr & DE_DIR) {   /* 是目录，就需要递归扫描 */
                printk("%s/%s\n", path, de.d_name);
                /* 构建新的路径 */
                i = strlen(path);
                sprintf(&path[i], "/%s", de.d_name);
                if (fsal_list_dir(path) < 0)
                    break;
                path[i] = 0;
            } else {    /* 直接列出文件 */
                printk("%s/%s  size=%d\n", path, de.d_name, de.d_size);
            }
        }
        fsif.closedir(dir);
    }
    //printk("opendir %s failed!.\n", path);
    return dir;
}

int init_disk_mount()
{
    char name[32];
    /* 选择一个已经创建文件系统的磁盘进行挂载为根目录 */
    int i;
    for (i = 0; i < 4; i++) {
        memset(name, 0, 32);
        strcpy(name, "disk");
        char s[2] = {0, 0};
        s[0] = i + '0';
        strcat(name, s);
        /* 挂载文件系统 */
        if (fsif.mount(name, ROOT_DIR_PATH, "fat32", 0) < 0) {
            #ifdef DEBUG_FSAL
            printk("[fsal] : mount on device %s failed!\n");
            #endif
            continue;
        }
        printk("[fsal] : mount device %s success!\n", name);
        break;  // 成功挂载
    }
    if (i >= 4) {
        printk("[fsal] : mount path %s failed!\n", ROOT_DIR_PATH);
        return -1;
    }


    return 0;
}

int init_fsal()
{
    if (init_fsal_file_table() < 0) {
        return -1;
    }

    if (init_fsal_dir_table() < 0) {
        return -1;
    }
    
    if (init_fsal_path_table() < 0) {
        return -1;
    }

    if (init_disk_mount() < 0) {
        return -1;
    }
    
    char path[MAX_PATH] = {0};
    strcpy(path, "/root");
    fsal_list_dir(path);

    return 0;
}


int fs_fd_init(task_t *task)
{
    task->fileman = kmalloc(sizeof(file_man_t));
    if (task->fileman == NULL) {
        return -1;
    }
    int i;
    for (i = 0; i < LOCAL_FILE_OPEN_NR; i++) {
        task->fileman->fds[i].handle = -1; /* no file */
        task->fileman->fds[i].flags = 0; /* no file */
        task->fileman->fds[i].offset = 0;
        
    }
    memset(task->fileman->cwd, 0, MAX_PATH);
    strcpy(task->fileman->cwd, "/");

    return 0;
}

int fs_fd_exit(task_t *task)
{
    if (!task->fileman)
        return -1;
    int i;
    for (i = 0; i < LOCAL_FILE_OPEN_NR; i++)
        fsif_degrow(i);
    
    kfree(task->fileman);
    task->fileman = NULL;
    return 0;
}

int fs_fd_copy(task_t *src, task_t *dest)
{
    if (!src->fileman || !dest->fileman) {
        return -1;
    }
    #ifdef DEBUG_FSAL
    printk("[fs]: fd copy from %s to %s\n", src->name, dest->name);
    #endif
    /* 复制工作目录 */
    memcpy(dest->fileman->cwd, src->fileman->cwd, MAX_PATH);
    int i;
    for (i = 0; i < LOCAL_FILE_OPEN_NR; i++) {
        if (src->fileman->fds[i].flags != 0) {
            dest->fileman->fds[i].handle = src->fileman->fds[i].handle;
            dest->fileman->fds[i].flags = src->fileman->fds[i].flags;
            dest->fileman->fds[i].offset = src->fileman->fds[i].offset;
            
            #ifdef DEBUG_FSAL
            printk("[fs]: fds[%d]=%d\n", i, src->fileman->fds[i].handle);
            #endif

            /* 增加资源的引用计数 */
            fsif_grow(i);
        }
    }
    return 0;
}

/**
 * fs_fd_reinit - 重新初始化只保留前3个fd
 * 
 */
int fs_fd_reinit(task_t *cur)
{
    if (!cur->fileman) {
        return -1;
    }
    /* 从第4个项开始需要关闭掉 */
    int i;
    for (i = 0; i < LOCAL_FILE_OPEN_NR; i++) {
        if (cur->fileman->fds[i].flags != 0) {  /* 已经占用，需要关闭 */
            #ifdef DEBUG_FSAL
            pr_dbg("[FS]: %s: fd=%d, flags=%x handle=%d\n", __func__, i, 
                cur->fileman->fds[i].flags, cur->fileman->fds[i].handle);
            #endif
            if (i >= 3)
                sys_close(i);
        }
    }
    return 0;
}

int fd_alloc()
{
    task_t *cur = current_task;
    int i;
    for (i = 0; i < LOCAL_FILE_OPEN_NR; i++) {
        if (cur->fileman->fds[i].flags == 0) {
            cur->fileman->fds[i].flags = 1;     /* alloced */
            cur->fileman->fds[i].handle = -1;
            cur->fileman->fds[i].offset = 0;
            
            return i;
        }
    }
    return -1;
}

int fd_free(int fd)
{
    task_t *cur = current_task;
    if (OUT_RANGE(fd, 0, LOCAL_FILE_OPEN_NR))
        return -1;
    if (cur->fileman->fds[fd].flags == 0) {
        return -1;
    }
    cur->fileman->fds[fd].handle = -1;
    cur->fileman->fds[fd].flags = 0;
    cur->fileman->fds[fd].offset = 0;
    
    return 0;
}

/**
 * local_fd_install - 安装到进程本地文件描述符表
 * @resid: 资源信息
 * @flags: 安装标志，一般用于确定是什么类型的资源
 */
int local_fd_install(int resid, unsigned int flags)
{
    if (OUT_RANGE(resid, 0, FSAL_FILE_OPEN_NR))
        return -1;

    int fd = fd_alloc();
    if (fd < 0)
        return -1;
    task_t *cur = current_task;
    cur->fileman->fds[fd].handle = resid;
    cur->fileman->fds[fd].flags |= flags;
    #ifdef DEBUG_FSAL
    printf("[FS]: %s: install fd=%d handle=%d\n", __func__, fd, resid);
    #endif
    return fd;
}

/**
 * local_fd_install - 安装到进程本地文件描述符表
 * @resid: 资源信息
 * @flags: 安装标志，一般用于确定是什么类型的资源
 */
int local_fd_install_to(int resid, int newfd, unsigned int flags)
{
    if (OUT_RANGE(resid, 0, FSAL_FILE_OPEN_NR))
        return -1;

    if (OUT_RANGE(newfd, 0, LOCAL_FILE_OPEN_NR))
        return -1;

    task_t *cur = current_task;
    cur->fileman->fds[newfd].handle = resid;
    cur->fileman->fds[newfd].flags = FILE_FD_ALLOC;
    cur->fileman->fds[newfd].flags |= flags;
    cur->fileman->fds[newfd].offset = 0;
    #ifdef DEBUG_FSAL
    printf("[FS]: %s: install fd=%d handle=%d\n", __func__, newfd, resid);
    #endif
    return newfd;
}

int local_fd_uninstall(int local_fd)
{
    if (OUT_RANGE(local_fd, 0, LOCAL_FILE_OPEN_NR))
        return -1;
    return fd_free(local_fd);
}

file_fd_t *fd_local_to_file(int local_fd)
{
    if (OUT_RANGE(local_fd, 0, LOCAL_FILE_OPEN_NR))
        return NULL;

    task_t *cur = current_task;
    return &cur->fileman->fds[local_fd];
}

int handle_to_local_fd(int handle, unsigned int flags)
{
    task_t *cur = current_task;
    file_fd_t *fdptr;
    int i;
    for (i = 0; i < LOCAL_FILE_OPEN_NR; i++) {
        fdptr = &cur->fileman->fds[i];
        if ((fdptr->handle == handle) && (fdptr->flags & flags)) {
            return i;   /* find the local fd */
        }
    }
    return -1;
}
 