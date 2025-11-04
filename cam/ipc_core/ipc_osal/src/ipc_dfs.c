#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/vfs.h>  
#include <dirent.h>

#include "ipc_dfs.h"

/******************************** file *************************************/

s32 ipc_file_seek(ipc_file_p h_file, s32 offset, ipc_file_seek_e whence)
{
    if (!h_file || h_file->fd < 0 || whence > IPC_SEEK_TAIL) {
        ipcerror("Seek file parameter error! h_file=[%p], whence=[%d]", h_file, whence);
        return IPC_INVALID_ARGS;
    }

    ipc_log_extend(h_file->fa_log);

    u8 seek_map[] = {
        [IPC_SEEK_HEAD] = SEEK_SET,
        [IPC_SEEK_CUR]  = SEEK_CUR,
        [IPC_SEEK_TAIL] = SEEK_END,
    };

    s32 now_offset = lseek(h_file->fd, offset, seek_map[whence]);
    ipcdebug("Seek file->fd=[%d] to offset=[%d]!", h_file->fd, now_offset);
    return now_offset;
}

s32 ipc_file_open(ipc_file_p h_file, pv8 path, ipc_file_mode_e mode, ipc_log_p fa_log)
{
    clog_init("dfs", "About directories, files, partitions");
    ipc_log_extend(fa_log);

    // coverity[NO_EFFECT :SUPPRESS]
    if (!h_file || !path || !path[0] || mode < 0 || mode > IPC_FILE_APPEND) {
        ipcerror("Open file parameter error! h_file=[%p], path=[%s], mode=[%d]", h_file, path, mode);
        return IPC_INVALID_ARGS;
    }

    s32 mode_map[] = {
        [IPC_FILE_RDONLY] = O_RDONLY,
        [IPC_FILE_WRONLY] = O_WRONLY | O_CREAT,
        [IPC_FILE_RDWR]   = O_RDWR   | O_CREAT,
        [IPC_FILE_APPEND] = O_WRONLY | O_CREAT | O_APPEND,
    };
    
    h_file->fd = open(path, mode_map[mode], 0777);
    if (h_file->fd < 0) {
        ipcwarn("Open file:[%s] failed! errmsg=[%s]", path, strerror(errno));
        return IPC_OPEN_ERROR;
    }

    h_file->fa_log = fa_log;

    ipcdebug("Open file:[%s]->fd=[%d] success!", path, h_file->fd);

    return IPC_SUCCESS;
}

void ipc_file_close(ipc_file_p h_file)
{
    if (!h_file || h_file->fd < 0) return ;

    ipc_log_extend(h_file->fa_log);
    ipcdebug("Close file->fd=[%d] success!", h_file->fd);

    close(h_file->fd);
    h_file->fd = -1;
}

s32 ipc_file_read(ipc_file_p h_file, pv8 buff, s32 max)
{
    if (!h_file || h_file->fd < 0 || !buff || max <= 0) {
        ipcerror("Read file parameter error! h_file=[%p], buff=[%p], max=[%d]", h_file, buff, max);
        return IPC_INVALID_ARGS;
    }

    ipc_log_extend(h_file->fa_log);

    s32 recv_len = 0;
	s32 recv_all = 0;
	
    while (recv_all < max) {
		recv_len = read(h_file->fd, buff+recv_all, max-recv_all);
		if (recv_len < 0) {
			if (errno == EINTR) continue; /* Interrupt, retry */
            ipcerror("Read file->fd=[%d] failed! Errmsg=[%s]", h_file->fd, strerror(errno));
			return IPC_READ_ERROR;
        }

		if (recv_len == 0) {
            ipcdebug("Read file->fd=[%d] finish", h_file->fd);
            break; /* Read complete */
        }
        
		recv_all += recv_len;
	}

    ipctrace(buff, recv_all);

    return recv_all;
}

s32 ipc_file_write(ipc_file_p h_file, pv8 buff, s32 len)
{
    if (!h_file || h_file->fd < 0 || !buff || len <= 0) {
        ipcerror("Write file parameter error! h_file=[%p], buff=[%p], len=[%d]", h_file, buff, len);
        return IPC_INVALID_ARGS;
    }

    ipc_log_extend(h_file->fa_log);

    s32 write_len = 0;
    s32 write_all = 0;

    // coverity[INTEGER_OVERFLOW :SUPPRESS]
    while (write_all < len) {
        // coverity[INTEGER_OVERFLOW :SUPPRESS]
        s32 remaining_len = len - write_all;
        if ((remaining_len <= 0) || (remaining_len > len - write_all)) {
            ipcerror("Internal error: remaining length calculation failed");
            return IPC_WRITE_ERROR;
        }

        write_len = write(h_file->fd, buff + write_all, remaining_len);
        if (write_len <= 0) {
            if (errno == EINTR)
                continue;
            ipcerror("Write file->fd=[%d] failed! Errmsg=[%s]", h_file->fd, strerror(errno));
            return IPC_WRITE_ERROR;
        }
        write_all += write_len;
    }

    // coverity[INTEGER_OVERFLOW :SUPPRESS]
    return (s32)write_all;
}

s32 ipc_file_clear(ipc_file_p h_file)
{
    if (!h_file || h_file->fd < 0) {
        ipcerror("Clear file parameter error! h_file=[%p]", h_file);
        return IPC_INVALID_ARGS;
    }

    s32 ret = ftruncate(h_file->fd, 0);
    if (ret != 0) {
        ipcerror("Ftruncate file failed! Retcode=[%d], errmsg=[%s]", ret, strerror(errno));
        return IPC_FAILED;
    }

    return ipc_file_seek(h_file, 0, IPC_SEEK_HEAD);
}

/****************************************************************/

typedef struct {
    s32 max;
    ipc_file_t h_file;
} fiter_t, *fiter_p;

static void _file_iter_uninit(ipc_iter_p h_iter)
{
    fiter_p fiter = (fiter_p)h_iter->private;
    ipc_file_close(fiter->h_file);
}

s32 ipc_file_read_iter(ipc_iter_p h_iter, pv8 path, pv8 buf, ps32 p_len)
{
    s32 ret = 0;
    fiter_p fiter = (fiter_p)h_iter->private;

    if (!h_iter->f_iter_uninit) {
        ret = ipc_file_open(fiter->h_file, path, IPC_FILE_RDONLY, h_iter->fa_log);
        if (ret < 0) goto FINISH;
        h_iter->f_iter_uninit = _file_iter_uninit;
        fiter->max = *p_len;
    }

    ret = ipc_file_read(fiter->h_file, buf, fiter->max);
    if (ret <= 0) goto FINISH;

    *p_len = ret;
    return IPC_ITER_CONTINUE;

FINISH :
    h_iter->ret = ret;
    ipc_iter_break_off(h_iter);
    return IPC_ITER_BREAK; /* Exit */
}

s32 ipc_file_write_iter(ipc_iter_p h_iter, pv8 path, pv8 buf, s32 len)
{
    s32 ret = 0;
    fiter_p fiter = (fiter_p)h_iter->private;

    if (!h_iter->f_iter_uninit) {
        ret = ipc_file_open(fiter->h_file, path, IPC_FILE_WRONLY, h_iter->fa_log);
        if (ret < 0) goto FINISH;
        h_iter->f_iter_uninit = _file_iter_uninit;
    }

    ret = ipc_file_write(fiter->h_file, buf, len);
    if (ret <= 0) goto FINISH;

    return IPC_ITER_CONTINUE;

FINISH :
    h_iter->ret = ret;
    ipc_iter_break_off(h_iter);
    return IPC_ITER_BREAK; /* Exit */
}

s32 ipc_file_copy(pv8 file_src, pv8 file_desc, ipc_log_p __IPC_LOG__)
{
    v8 buf[128] = {0};
    s32 len = sizeof(buf);
    ITER_INIT(iter, 2);
    while (ipc_file_read_iter (iter[0], file_src , buf, &len)
        && ipc_file_write_iter(iter[1], file_desc, buf, len));

    return ipc_iter_retval(iter);
}

s32 ipc_file_write_once(pv8 path, pv8 buff, s32 len, ipc_log_p fa_log)
{
    ipc_file_t h_file;
    s32 ret = ipc_file_open(h_file, path, IPC_FILE_WRONLY, fa_log);
    if (ret < 0) return ret;

    ret = ipc_file_write(h_file, buff, len);
    ipc_file_close(h_file);

    return ret;
}

s32 ipc_file_read_once(pv8 path, pv8 buff, s32 max, ipc_log_p fa_log)
{
    ipc_file_t h_file;
    s32 ret = ipc_file_open(h_file, path, IPC_FILE_RDONLY, fa_log);
    if (ret < 0) return ret;

    ret = ipc_file_read(h_file, buff, max);
    ipc_file_close(h_file);

    return ret;
}

s32 ipc_file_append_once(pv8 path, pv8 buff, s32 len, ipc_log_p fa_log)
{
    ipc_file_t h_file;
    s32 ret = ipc_file_open(h_file, path, IPC_FILE_APPEND, fa_log);
    if (ret < 0) return ret;

    ret = ipc_file_write(h_file, buff, len);
    ipc_file_close(h_file);
    
    return ret;
}

/******************************** dir *************************************/

s32 ipc_mkdirs(pv8 path)
{
    if (!path || !path[0]) return IPC_INVALID_ARGS;

    v8  dest_path[256] = {0};
    s32 dest_idx = 0;
    s32 src_idx  = 0;
    s32 ret = 0;

    while(1) {
        dest_path[dest_idx] = path[src_idx];

        if (path[src_idx] == '\0') {
            // CID 21739: Replace access+mkdir with single mkdir call
            // coverity[TOCTOU : SUPPRESS]
            ret = mkdir(dest_path, 0777);
            // Check if directory already exists (EEXIST) or was successfully created
            if (ret != 0 && errno != EEXIST) return IPC_WRITE_ERROR;
            break;
        }

        if (path[src_idx] == '/') {
            // CID 21739: Replace access+mkdir with single mkdir call
            // coverity[TOCTOU : SUPPRESS]
            ret = mkdir(dest_path, 0777);
            // Check if directory already exists (EEXIST) or was successfully created
            if (ret != 0 && errno != EEXIST) return IPC_WRITE_ERROR;
            while(path[src_idx] == '/') src_idx++;
        } else {
            src_idx++;
        }

        dest_idx++;
    }

    return IPC_SUCCESS;
}

s32 ipc_rm(pv8 path)
{
    // coverity[TOCTOU :SUPPRESS]
    if (!path || !path[0]) return IPC_INVALID_ARGS;

    struct stat stat_info;

    // coverity[TOCTOU :SUPPRESS]
    s32 ret = lstat(path, &stat_info);
    if (ret != 0) return IPC_NOT_FOUND;

    if (!S_ISDIR(stat_info.st_mode))  {
        return unlink(path) != 0 ? IPC_FAILED : IPC_SUCCESS;
    }

    // coverity[TOCTOU :SUPPRESS]
    DIR *dir = opendir(path);
    if (dir == NULL) return IPC_OPEN_ERROR;

    v8 sub_file[384];
    struct dirent *file = NULL;
    while((file = readdir(dir))) {
        if (!strcmp(file->d_name, ".") || !strcmp(file->d_name, "..")) continue;
        snprintf(sub_file, sizeof(sub_file), "%s/%s", path, file->d_name);
        ret = ipc_rm(sub_file);
        if (ret < 0) break;
    }
    closedir(dir);

    return ret < 0 ? ret : rmdir(path) != 0 ? IPC_FAILED : IPC_SUCCESS;
}

/******************************** partition *************************************/

s32 ipc_df(pv8 path, pu32 total_k, pu32 used_k, pu32 free_k)
{
    if (!path || !path[0]) return IPC_INVALID_ARGS;

    struct statfs df;
    s32 ret = statfs(path, &df);
    if (ret != 0) return IPC_FAILED;

    df.f_bsize >>= 10;
    if (total_k) *total_k = df.f_blocks * df.f_bsize;
    if (used_k)  *used_k  = (df.f_blocks - df.f_bfree) * df.f_bsize;
    if (free_k)  *free_k  = df.f_bavail * df.f_bsize;

    return IPC_SUCCESS;
}

