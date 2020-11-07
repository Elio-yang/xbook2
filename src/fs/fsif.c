#include <fsal/fsal.h>
#include <xbook/fs.h>
#include <xbook/task.h>
#include <xbook/debug.h>
#include <xbook/driver.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <lwip/sockets.h>
#include <xbook/fifo.h>
#include <xbook/pipe.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>

// #define DEBUG_FSIF

int sys_open(const char *path, int flags)
{
    int handle;
    int fd = -1;
    unsigned long new_flags;
    if (O_DEVEX & flags) {
        /* 去掉根目录 */
        char *p = (char *) path;
        if (*p == '/')
            p++;
        handle = device_open(p, flags);
        if (handle < 0)
            return -1;
        fd = local_fd_install(handle, FILE_FD_DEVICE);
    } else if (O_FIFO & flags) {
        /* 去掉根目录 */
        char *p = (char *) path;
        if (*p == '/')
            p++;
        
        /* 有创建标志 */
        new_flags = IPC_CREAT;
        if (flags & O_CREAT) {
            new_flags |= IPC_EXCL;
        }
        if (flags & O_RDWR) {
            new_flags |= (IPC_READER | IPC_WRITER);
        } else if (flags & O_RDONLY) {
            new_flags |= IPC_READER;
        } else if (flags & O_WRONLY) {
            new_flags |= IPC_WRITER;
        }
        if (flags & O_NONBLOCK) {
            new_flags |= IPC_NOWAIT;
        }
        handle = fifo_get(p, new_flags);
        if (handle < 0)
            return -1;
        fd = local_fd_install(handle, FILE_FD_FIFO);
    } else {
        //printk("[fs]: %s: path=%s flags=%x\n", __func__, path, flags);
        handle = fsif.open((void *)path, flags);
        if (handle < 0)
            return -1;
        fd = local_fd_install(handle, FILE_FD_NORMAL);
    }
    return fd;
}

int sys_close(int fd)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;

    if (ffd->flags & FILE_FD_NORMAL) {
        if (fsif.close(ffd->handle) < 0)
            return -1;    
    } else if (ffd->flags & FILE_FD_SOCKET) {
        #if DEBUG_FSIF
        printk("[FS]: %s: close fd %d socket %d.\n", __func__, fd, ffd->handle);
        #endif
        if (lwip_close(ffd->handle) < 0)
            return -1;
    } else if (ffd->flags & FILE_FD_DEVICE) {
        if (device_close(ffd->handle) < 0)
            return -1;
    } else if (ffd->flags & FILE_FD_FIFO) {
        if (fifo_put(ffd->handle) < 0)
            return -1;        
    } else if (ffd->flags & FILE_FD_PIPE0) {
        if (pipe_close(ffd->handle, 0) < 0)
            return -1;  
    } else if (ffd->flags & FILE_FD_PIPE1) {
        if (pipe_close(ffd->handle, 1) < 0)
            return -1;
    }
    return local_fd_uninstall(fd);
}

int sys_read(int fd, void *buffer, size_t nbytes)
{
    // printk("[fs]: %s: fd=%d buf=%x bytes=%d\n", __func__, fd, buffer, nbytes);
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0) {
        pr_err("[FS]: %s: fd %d err!\n", __func__, fd);
        return -1;
    }
    int retval = -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        retval = fsif.read(ffd->handle, buffer, nbytes);
    } else if (ffd->flags & FILE_FD_SOCKET) {
        retval = lwip_read(ffd->handle, buffer, nbytes);  
    } else if (ffd->flags & FILE_FD_DEVICE) {
        retval = device_read(ffd->handle, buffer, nbytes, ffd->offset);  
        if (retval > 0)
            ffd->offset += (retval / SECTOR_SIZE);
    } else if (ffd->flags & FILE_FD_FIFO) {
        retval = fifo_read(ffd->handle, buffer, nbytes, 0);  
    } else if ((ffd->flags & FILE_FD_PIPE0)) {
        retval = pipe_read(ffd->handle, buffer, nbytes);  
    }
    return retval;
}

int sys_write(int fd, void *buffer, size_t nbytes)
{
    //pr_dbg("sys_write: %s\n", buffer);

    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0) {
        pr_err("[FS]: %s: fd %d err! handle=%d flags=%x\n", __func__, 
            fd, ffd->handle, ffd->flags);
        return -1;
    }
    int retval = -1;
    
    if (ffd->flags & FILE_FD_NORMAL) {
        retval = fsif.write(ffd->handle, buffer, nbytes);
    } else if (ffd->flags & FILE_FD_SOCKET) {
        /* 由于lwip_write实现原因，需要内核缓冲区中转 */
        void *tmpbuffer = kmalloc(nbytes);
        if (tmpbuffer == NULL)
            return -1;
        memcpy(tmpbuffer, buffer, nbytes);
        retval = lwip_write(ffd->handle, tmpbuffer, nbytes);  
        kfree(tmpbuffer);
    } else if (ffd->flags & FILE_FD_DEVICE) {
        retval = device_write(ffd->handle, buffer, nbytes, ffd->offset);  
        if (retval > 0)
            ffd->offset += (retval / SECTOR_SIZE);
    } else if (ffd->flags & FILE_FD_FIFO) {
        retval = fifo_write(ffd->handle, buffer, nbytes, 0);  
    } else if ((ffd->flags & FILE_FD_PIPE1)) {
        retval = pipe_write(ffd->handle, buffer, nbytes);  
    }
    return retval;
}

int sys_ioctl(int fd, int cmd, unsigned long arg)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        return fsif.ioctl(ffd->handle, cmd, arg);
    } else if (ffd->flags & FILE_FD_DEVICE) {
        return device_devctl(ffd->handle, cmd, arg);
    } else if (ffd->flags & FILE_FD_FIFO) {
        return fifo_ctl(ffd->handle, cmd, arg);
    } else if (ffd->flags & FILE_FD_PIPE0) {
        return pipe_ioctl(ffd->handle, cmd, arg, 0);
    } else if (ffd->flags & FILE_FD_PIPE1) {
        return pipe_ioctl(ffd->handle, cmd, arg, 1);
    }
    return -1;
}

int sys_fcntl(int fd, int cmd, long arg)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        return fsif.fcntl(ffd->handle, cmd, arg);
    } else if (ffd->flags & FILE_FD_SOCKET) {
        return lwip_fcntl(ffd->handle, cmd, arg);  
    } else if (ffd->flags & FILE_FD_PIPE0) {
        return pipe_ioctl(ffd->handle, cmd, arg, 0);
    } else if (ffd->flags & FILE_FD_PIPE1) {
        return pipe_ioctl(ffd->handle, cmd, arg, 1);
    }
    return -1;
}

int sys_lseek(int fd, off_t offset, int whence)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        return fsif.lseek(ffd->handle, offset, whence);
    } else if (ffd->flags & FILE_FD_DEVICE) {
        ffd->offset = offset;
        return 0;
    }
    return -1;
}

int sys_access(const char *path, int mode)
{
    //printk("%s: path: %s\n", __func__, path);
    return fsif.access(path, mode);
}

int sys_unlink(const char *path)
{
    return fsif.unlink((char *) path);
}

int sys_ftruncate(int fd, off_t offset)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        return fsif.ftruncate(ffd->handle, offset);
    }
    return -1;
}

int sys_fsync(int fd)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        return fsif.fsync(fd);
    }
    return -1;
}

long sys_tell(int fd)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        return fsif.ftell(fd);
    }
    return -1;
}

int sys_stat(const char *path, struct stat *buf)
{
    return fsif.state((char *) path, buf);
}

int sys_fstat(int fd, struct stat *buf)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        return fsif.fstat(ffd->handle, buf);
    }
    return -1;
}

int sys_chmod(const char *path, mode_t mode)
{
    return fsif.chmod((char *) path, mode);
}

int sys_fchmod(int fd, mode_t mode)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        return fsif.fchmod(ffd->handle, mode);
    }
    return -1;
}

int sys_mkdir(const char *path, mode_t mode)
{
    return fsif.mkdir((char *) path, mode);
}

int sys_rmdir(const char *path)
{
    return fsif.rmdir((char *) path);
}

int sys_rename(const char *source, const char *target)
{
    return fsif.rename((char *) source, (char *) target);
}

int sys_chdir(const char *path)
{
    task_t *cur = current_task;
    if (!cur->fileman)
        return -1;
    
    //printk("%s: path:%s\n", __func__, path);
    /* 打开目录 */
    dir_t dir = sys_opendir(path);
    if (dir < 0) {
        return -1;
    }
    sys_closedir(dir);

    /* 保存路径 */
    int len = strlen(path);
    memset(cur->fileman->cwd, 0, MAX_PATH);
    memcpy(cur->fileman->cwd, path, min(len, MAX_PATH));
    return 0;
}

int sys_getcwd(char *buf, int bufsz)
{
    task_t *cur = current_task;
    if (!cur->fileman)
        return -1;
    memcpy(buf, cur->fileman->cwd, min(bufsz, MAX_PATH));
    return 0;
}

dir_t sys_opendir(const char *path)
{
    return fsif.opendir((char *) path);
}
int sys_closedir(dir_t dir)
{
    return fsif.closedir(dir);
}

int sys_readdir(dir_t dir, struct dirent *dirent)
{
    return fsif.readdir(dir, dirent);
}

int sys_rewinddir(dir_t dir)
{
    return fsif.rewinddir(dir);
}

/**
 * @fd: 文件描述符
 * 
 * 增长文件对于的引用
 * 
 * 成功返回0，失败返回-1
 */
int fsif_grow(int fd)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        
    } else if (ffd->flags & FILE_FD_DEVICE) {
        if (device_grow(ffd->handle) < 0)
            return -1;
        
    } else if (ffd->flags & FILE_FD_FIFO) {
        if (fifo_grow(ffd->handle) < 0)
            return -1;
    } else if (ffd->flags & FILE_FD_PIPE0) {
        if (pipe_grow(ffd->handle, 0) < 0)
            return -1;
    } else if (ffd->flags & FILE_FD_PIPE1) {
        if (pipe_grow(ffd->handle, 1) < 0)
            return -1;
    } else if (ffd->flags & FILE_FD_SOCKET) {
        
    }
    return 0;
}

/**
 * @fd: 文件描述符
 * 
 * 增长文件对于的引用
 * 
 * 成功返回0，失败返回-1
 */
int fsif_degrow(int fd)
{
    file_fd_t *ffd = fd_local_to_file(fd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (ffd->flags & FILE_FD_NORMAL) {
        
    } else if (ffd->flags & FILE_FD_DEVICE) {
        if (device_degrow(ffd->handle) < 0)
            return -1;
    }
    return 0;
}

/**
 * 复制一个文件描述符
 * @oldfd: 需要复制的fd
 * 
 * 复制fd，把对应的资源的引用计数+1，然后安装到一个最小的fd中。
 * 
 * 成功返回fd，失败返回-1
 */
int sys_dup(int oldfd)
{
    file_fd_t *ffd = fd_local_to_file(oldfd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    int newfd = -1;
    /* 增长 */
    if (fsif_grow(oldfd) < 0)
        return -1;

    /* 安装 */
    if (ffd->flags & FILE_FD_NORMAL) {
        // newfd = local_fd_install(ffd->handle, FILE_FD_NORMAL);
    } else if (ffd->flags & FILE_FD_DEVICE) {
        newfd = local_fd_install(ffd->handle, FILE_FD_DEVICE);
    } else if (ffd->flags & FILE_FD_FIFO) {
        newfd = local_fd_install(ffd->handle, FILE_FD_FIFO);
    } else if (ffd->flags & FILE_FD_SOCKET) {

    } else if (ffd->flags & FILE_FD_PIPE0) {
        newfd = local_fd_install(ffd->handle, FILE_FD_PIPE0);
    } else if (ffd->flags & FILE_FD_PIPE1) {
        newfd = local_fd_install(ffd->handle, FILE_FD_PIPE1);
    }
    return newfd;
}

/**
 * 复制一个文件描述符
 * @oldfd: 需要复制的fd
 * @newfd: 需要复制到的fd
 * 
 * 复制fd，把对应的资源的引用计数+1，然后安装到newfd中，如果newfd已经打开，则需要先关闭
 * 如果oldfd和newfd一样，则直接返回newfd
 * 
 * 成功返回newfd，失败返回-1
 */
int sys_dup2(int oldfd, int newfd)
{
    file_fd_t *ffd = fd_local_to_file(oldfd);
    if (ffd == NULL || ffd->handle < 0 || ffd->flags == 0)
        return -1;
    if (oldfd == newfd) /* 一样则直接返回 */
        return newfd;
    /* 查看新fd，看是否已经打开，如果是，则先关闭。 */
    file_fd_t *newffd = fd_local_to_file(newfd);
    if (newffd != NULL && newffd->handle >= 0 && newffd->flags != 0) {
        //pr_dbg("[FS]: %s: new fd %d exist! close it.\n", __func__, newfd);    
        if (sys_close(newfd) < 0)   /* 关闭 */
            return -1;   
    }
        
    /* 复制oldfd并安装到newfd中 */
    if (fsif_grow(oldfd) < 0)
        return -1;
    
    /* 安装 */
    if (ffd->flags & FILE_FD_NORMAL) {
        // newfd = local_fd_install_to(ffd->handle, newfd, FILE_FD_NORMAL);
    } else if (ffd->flags & FILE_FD_DEVICE) {
        
        newfd = local_fd_install_to(ffd->handle, newfd, FILE_FD_DEVICE);
    } else if (ffd->flags & FILE_FD_FIFO) {
        
        newfd = local_fd_install_to(ffd->handle, newfd, FILE_FD_FIFO);
    } else if (ffd->flags & FILE_FD_SOCKET) {

    } else if (ffd->flags & FILE_FD_PIPE0) {
        newfd = local_fd_install_to(ffd->handle, newfd, FILE_FD_PIPE0);
    } else if (ffd->flags & FILE_FD_PIPE1) {
        newfd = local_fd_install_to(ffd->handle, newfd, FILE_FD_PIPE1);
    }
    return newfd;
}

int sys_pipe(int fd[2])
{
    if (!fd)
        return -1;
    
    pipe_t *pipe = create_pipe();
    if (pipe == NULL)
        return -1;
    /* read */
    int rfd = local_fd_install(pipe->id, FILE_FD_PIPE0);
    if (rfd < 0) {
        destroy_pipe(pipe);
        return -1;
    }
    /* write */
    int wfd = local_fd_install(pipe->id, FILE_FD_PIPE1);
    if (wfd < 0) {
        local_fd_uninstall(rfd);
        destroy_pipe(pipe);
        return -1;
    }
    fd[0] = rfd;
    fd[1] = wfd;
    return 0;
}


int sys_mount(
    char *source,         /* 需要挂载的资源 */
    char *target,         /* 挂载到的目标位置 */
    char *fstype,         /* 文件系统类型 */
    unsigned long mountflags    /* 挂载标志 */
) {
    return fsif.mount(source, target, fstype, mountflags);
}

int sys_unmount(char *path, unsigned long flags)
{
    return fsif.unmount(path, flags);
}

int sys_mkfs(char *source,         /* 需要创建FS的设备 */
    char *fstype,         /* 文件系统类型 */
    unsigned long flags   /* 标志 */
) {
    return fsif.mkfs(source, fstype, flags);
}

int sys_probe(const char *name, int flags, char *buf, size_t buflen)
{
    if (flags & O_DEVEX) {
        return device_probe_unused(name, buf, buflen);
    } else {
        printk("%s: do not support probe!\n");
    }
    return -1;
}
