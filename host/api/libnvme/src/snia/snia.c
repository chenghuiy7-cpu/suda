

#include "snia_util.h"

#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdatomic.h>
#include "snia.h"
#include "libnvme.h"
#include "liburing.h"
#include <linux/fs.h>
#include <linux/fiemap.h>
#include <mntent.h>
#include <fcntl.h>
#include <sys/sysmacros.h>
#define FIEMAP_MAX_EXTENTS 32
/* Set to 1 to route compute requests through user space */
#define ROUTE_CS_COMPUTE_THROUGH_USER_SPACE 0

typedef void *PHYSICAL_ADDR;

#define __CS_PLACE_HOLDER_DEV_NAME "VSCODE_Device"
#define __CS_PLACE_HOLDER_CSE_NAME "VSCODE_FUSION_CSE"

static int is_chardev(struct stat s)
{
    return S_ISCHR(s.st_mode);
}

static int is_blkdev(struct stat s)
{
    return S_ISBLK(s.st_mode);
}

static struct vscode_dev_descriptor
{
    CS_DEV_HANDLE admin_dev_handle;
    CS_DEV_HANDLE compute_dev_handle;
    CS_DEV_HANDLE uring_compute_dev_handle;
    struct io_uring* ring;//uring for async cmd submit
    int ring_num;
    int pipe_fd[2];
    pthread_t async_thread;
} dev_handler_register[256];

struct uring_cmd_callback_context{
    void* context;
    int cmd_num;
    struct nvme_uring_cmd *cmd;
    struct iovec *iovecs;
    csQueueCallbackFn CallbackFn;

};
static struct
{
    CS_MEM_HANDLE mem_handle;
    CS_DEV_HANDLE cs_dev_handle;
} mem_handler_register[256];

static atomic_int dev_handler_allocator = 0;
static atomic_int mem_handler_allocator = 0;

int test_nvme_io_fixed(void *ptr)
{
    struct io_uring *ring = ptr;
    printf("Begin To Open NVME Char dev\n");
    int fd = open("/dev/ng0n1", O_WRONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Failed to Open NG0N1 Device\n");
        return 1;
    }
    struct io_uring_cqe *cqe;
    struct io_uring_sqe *sqe;
    struct iovec vecs[2];
    int i, ret;

    posix_memalign(&vecs[0].iov_base, 4096, 4096);
    posix_memalign(&vecs[1].iov_base, 4096, 4096);
    struct nvme_uring_cmd *cmd = vecs[0].iov_base;
    vecs[0].iov_len = sizeof(struct nvme_uring_cmd);

    cmd->opcode = 0x2;
    cmd->data_len = 4096;
    cmd->addr = (unsigned long long)(vecs[1].iov_base);
    cmd->nsid = 1;
    cmd->cdw10 = 0;
    cmd->cdw11 = 0;
    cmd->flags = 0;
    cmd->cdw12 = 0;
    cmd->cdw13 = 0;
    cmd->cdw14 = 0;
    cmd->cdw15 = 0;

    ret = io_uring_register_buffers(ring, vecs, 1);
    if (ret)
    {
        fprintf(stderr, "Failed to register buffers: %d\n", ret);
        return 1;
    }

    sqe = io_uring_get_sqe(ring);
    if (!sqe)
    {
        fprintf(stderr, "get sqe failed\n");
        goto err;
    }
    io_uring_prep_write_fixed(sqe, fd, vecs[0].iov_base,
                              vecs[0].iov_len, 0, 0);
    sqe->user_data = 1;

    ret = io_uring_submit(ring);

    if (ret < 0)
    {
        fprintf(stderr, "sqe submit failed: %d\n", ret);
        goto err;
    }
    else if (ret != 1)
    {
        fprintf(stderr, "Submitted Failed ret%d\n", ret);
        goto err;
    }

    for (i = 0; i < 1; i++)
    {
        ret = io_uring_wait_cqe(ring, &cqe);
        if (ret < 0)
        {
            fprintf(stderr, "wait completion %d\n", ret);
            goto err;
        }
        if (cqe->res < 0)
        {
            fprintf(stderr, "I/O write error on %lu: %s\n",
                    (unsigned long)cqe->user_data,
                    strerror(-cqe->res));
            goto err;
        }
        io_uring_cqe_seen(ring, cqe);
    }
    io_uring_unregister_buffers(ring);
    free(vecs[0].iov_base);
    return 0;
err:
    return 1;
}

static inline size_t get_request_size(CsComputeRequest *req)
{
    // The structure is allocated with at least one argument see 6.3.4.2.7
    if (req->NumArgs)
    {
        /* More memory past the structure is alloced for more arguments see :
           From section A1 :
            // allocate request buffer for 3 args
            req = calloc(1, sizeof(CsComputeRequest) + (sizeof(CsComputeArg) * 3));
            RWE : The calloc above allocates one sizeof(CsComputeArg) too much
                  because the structure already holds one in any case (6.3.4.2.7)
        */
        return sizeof(CsComputeRequest) + (req->NumArgs - 1) * sizeof(CsComputeArg);
    }
    else
    {
        /* If for some reason NumArgs is 0 return the size of the struct instead of
           sizeof(CsComputeRequest) + (0-1) * sizeof(CsComputeArg) !
        */
        return sizeof(CsComputeRequest);
    }
    /** @note if we suppose the calloc always allocates too much we can return :
     *  return sizeof(CsComputeRequest) + (req->NumArgs)*sizeof(CsComputeArg);
     *  In all cases, but this means we suppose one extra CsComputeArg was allocated
     *  which is the case in the example, but wouldn't be so sure this will always
     *  remain the case... */
}

CS_STATUS csQueryCSFList(const char *Path, int *Length, int *Count, CSFUniqueId *Buffer)
{
    if (!Path || !Length || !Count || strlen(Path) < 10)
        return CS_INVALID_ARG;
    bool data_overflow = false;
    if (strncmp(Path, "/dev/nvmq", 9) == 0)
    {
        // 检查后面是否全是数字
        char *num_part = Path + 9;
        bool is_valid = true;

        // 判断是否为纯数字
        for (int i = 0; num_part[i] != '\0'; i++)
        {
            if (!isdigit(num_part[i]))
            {
                is_valid = false;
                break;
            }
        }
        int ret = nvme_open(basename(Path));
        if (ret < 0)
        {
            printf("Could not open device : %s\n"
                   "Path %s do you have admin (sudo) rights ?",
                   basename(Path), Path);
            // return CS_ENOENT; // CS_ENOENT is not defined
            return CS_DEVICE_NOT_PRESENT;
        }
        unsigned long log_page_data[4096];
        memset(log_page_data, 0, 4096);
        union nvme_prog_desc_list *desclist = log_page_data;
        ret = nvme_get_nsid_log(ret, false, 0x82, 2, 4096, (void *)log_page_data);
        if (ret != 0 || desclist[0].header.numd != 48)
        {
            fprintf(stderr, "Failed to get program list numd%d\n", desclist[0].header.numd);
            return CS_INVALID_ARG;
        }
        int index = 0;
        for (int i = 0; i < 16; i++)
        {
            if (desclist[i + 1].data.peocc != 0)
            {
                if ((*Length) < ((index + 1) * sizeof(CSFUniqueId)))
                    data_overflow = true;
                if (Buffer != NULL && (!data_overflow))
                {
                    snprintf(Buffer[index].UniqueName, 32, "PROG_%d_%s", i,
                             desclist[i + 1].data.program_type == fusion_program ? "FUSION" : desclist[i + 1].data.program_type == xilinx_fpga_only_program ? "HW"
                                                                                          : desclist[i + 1].data.program_type == xilinx_soc_only_program    ? "SW"
                                                                                                                                                            : "UNKNOWN");
                    printf("Get Buffer Name%s\n", Buffer[index].UniqueName);
                    Buffer[index].GlobalId = i;
                }
                ++index;
            }
        }
        if ((*Length) < ((index + 1) * sizeof(CSFUniqueId)))
            data_overflow = true;
        if (Buffer != NULL && data_overflow)
        {
            snprintf(Buffer[index].UniqueName, 32, "MEM_SET_OP");
            Buffer[index].GlobalId = VENDOR_DEFINED_MEMRANGE_ID;
            ++index;
        }
        for (int i = 0; i < 32; i++)
        {
            if (desclist[i + 17].data.pit != 0)
            {
                char op_name[9];
                for (int j = 0; j < 8; j++)
                {
                    op_name[j] = desclist[i + 17].data.pid;
                    desclist[i + 17].data.pid = desclist[i + 17].data.pid >> 8;
                }
                op_name[8] = '\0';
                if ((*Length) < ((index + 1) * sizeof(CSFUniqueId)))
                    data_overflow = true;
                if (Buffer != NULL && !data_overflow)
                {
                    snprintf(Buffer[index].UniqueName, 32, "OP_%s", op_name);
                    Buffer[index].GlobalId = i;
                }
                ++index;
            }
        }
        *Length = index * sizeof(CSFUniqueId);
        *Count = index;
        if (Buffer != NULL && data_overflow)
        {
            return CS_INVALID_LENGTH;
        }
        close(ret);
    }
    else
        return CS_INVALID_ARG;
}

/**
 * @copydoc csGetCSxFromPath
 * */
CS_STATUS csGetCSxFromPath(char *Path, unsigned int *Length, char *DevName)
{
    // CS_ENOENT Not in file system
    // CS_ENTITY_NOT_ON_DEVICE normal NVMe (non compute capable)
    // CS_ENXIO failed to do IO
    int ret, fd;
    struct stat nvme_stat;

    // Check arguments
    if (!Path || !Length || !DevName)
    {
        return CS_INVALID_ARG;
    }
    struct stat path_stat;

    // 获取文件状态，如果失败返回错误
    if (stat(Path, &path_stat) != 0)
    {
        return CS_INVALID_ARG;
    }

    // 检查是否为常规文件
    if (!S_ISREG(path_stat.st_mode))
    {
        fprintf(stderr, "Current Does not support Search in dir!Only support specified file!\n");
        return CS_INVALID_ARG; // 不是常规文件返回错误
    }
    // Open device
    char *devicename = basename(Path);
    ret = nvme_open(devicename);
    if (ret < 0)
    {
        printf("Could not open device : %s\n"
               "Given by path %s, do you have admin (sudo) rights ?",
               devicename, Path);
        // return CS_ENOENT; // CS_ENOENT is not defined
        return CS_NO_SUCH_ENTITY_EXISTS;
    }
    fd = ret;

    // Check if device is char/blk
    ret = fstat(fd, &nvme_stat);
    if (ret < 0)
    {
        printf("Could not stat file descriptor for %s", Path);
        close(fd);
        /// @todo maybe inappropriate error code
        return CS_ENXIO;
    }

    if (!is_chardev(nvme_stat) && !is_blkdev(nvme_stat))
    {
        printf("%s is not a block or character device", Path);
        close(fd);
        /// @todo maybe inappropriate error code
        return CS_ENXIO;
    }

    // Copy the device name
    size_t len = strlen(devicename);
    if (*Length < len + 1)
    {
        close(fd);
        return CS_INVALID_LENGTH;
    }

    strncpy(DevName, devicename, len + 1);
    printf("Returned CSx device : %s from path : %s", DevName, Path);
    close(fd);
    return CS_SUCCESS;
}

CS_STATUS csQueryCSxList(int *length, char *Buffer)
{
    DIR *dir;
    struct dirent *entry;
    char *temp_buffer = Buffer; // 临时缓冲区
    memset(Buffer, 0, *length);
    int valid_count = 0; // 有效设备计数
    int valid_length = 0;
    // 打开/dev目录
    dir = opendir("/dev");
    if (dir == NULL)
    {
        *length = 0;
        return CS_DEVICE_NOT_PRESENT;
    }

    // 遍历目录
    while ((entry = readdir(dir)) != NULL)
    {
        char *name = entry->d_name;

        // 检查是否以"nvmq"开头
        if (strncmp(name, "nvmq", 4) == 0)
        {
            // 检查后面是否全是数字
            char *num_part = name + 4;
            bool is_valid = true;

            // 判断是否为纯数字
            for (int i = 0; num_part[i] != '\0'; i++)
            {
                if (!isdigit(num_part[i]))
                {
                    is_valid = false;
                    break;
                }
            }

            if (is_valid && strlen(num_part) > 0)
            {
                // 构造需要检查的设备名
                char ng_name[256];
                snprintf(ng_name, sizeof(ng_name), "/dev/ng%s", num_part);
                char nvmq_name[256];
                snprintf(nvmq_name, sizeof(nvmq_name), "/dev/nvmq%sn1", num_part);

                // 检查相应的设备文件是否存在
                if (access(ng_name, F_OK) == 0 && access(nvmq_name, F_OK) == 0)
                {
                    // 添加到临时缓冲区，如果不是第一个设备，加上逗号
                    if ((*length) - valid_length < (strlen(num_part) + (valid_count > 0)))
                    {
                        *length = valid_length;
                        return CS_INVALID_LENGTH;
                    }
                    if (valid_count > 0)
                    {
                        strcat(temp_buffer, ",");
                    }
                    strcat(temp_buffer, num_part);
                    valid_count++;
                    valid_length = strlen(temp_buffer);
                }
            }
        }
    }

    closedir(dir);

    // 复制结果到输出缓冲区
    if (valid_count > 0)
    {
        *length = valid_length;
        return CS_SUCCESS;
    }
    else
    {
        *length = 0;
        Buffer[0] = '\0';
        return CS_DEVICE_NOT_PRESENT;
    }
}

void vscode_async_thread(void* ctx){
    struct vscode_dev_descriptor* desc = ctx;
    while(1){
        struct uring_cmd_callback_context data;
        int nbytes = read(desc->pipe_fd[0],&data,sizeof(struct uring_cmd_callback_context));
        if(nbytes!=sizeof(struct uring_cmd_callback_context)){
            fprintf(stderr,"Failed To Read Data,Thread will dead!\n");
            return;
        }
        struct io_uring_cqe cqe;
        for(int i=0;i<data.cmd_num;i++)
        {
            int ret = io_uring_wait_cqe(&desc->ring,&cqe);
            if(ret < 0){
                fprintf(stderr,"io_uring_wait_cqe:%s\n",strerror(-ret));
                return;
            }
        }
        //TODO Use Pooling in the future
        free(data.cmd);
        free(data.iovecs);
        if(data.context!=NULL&&data.CallbackFn!=NULL){
            data.CallbackFn(data.context,CS_SUCCESS,0);
        }

    }
}

/**
 * @copydoc csOpenCSx
 * */
CS_STATUS csOpenCSx(char *DevName, void *DevContext,
                    CS_DEV_HANDLE *DevHandle)
{
    int ret, fd;

    if (!DevName || !DevHandle)
    {
        return CS_INVALID_ARG;
    }

    char *str = DevName;
    char str1[100];
    // 检查字符串长度是否至少为5位
    if (strlen(str) < 5)
    {
        return CS_NO_SUCH_ENTITY_EXISTS;
    }

    // 检查前4位是否为"nvmq"
    if (strncmp(str, "nvmq", 4) != 0)
    {
        return CS_NO_SUCH_ENTITY_EXISTS;
    }

    // 检查第5位及之后是否都是数字
    for (int i = 4; i < strlen(str); i++)
    {
        if (!isdigit(str[i]))
        {
            return CS_NO_SUCH_ENTITY_EXISTS;
        }
    }

    // 拷贝原字符串并添加"n1"
    strcpy(str1, str);
    strcat(str1, "n1");

    // Note : Checks have been performed in csGetCSxFromPath()
    /// @todo Maybe add checks here if called without the above
    ret = nvme_open(DevName);
    if (ret < 0)
    {
        printf("Could not open device : %s\n", DevName);
        // return CS_ENOENT; // CS_ENOENT is not defined
        return CS_NO_SUCH_ENTITY_EXISTS;
    }

    fd = ret;

    ret = nvme_open(str1);
    if (ret < 0)
    {
        printf("Could not open device : %s\n", DevName);
        // return CS_ENOENT; // CS_ENOENT is not defined
        return CS_NO_SUCH_ENTITY_EXISTS;
    }

    strcpy(str1, "/dev/ng");
    strcpy(str1 + 2, str + 4);
    strcat(str1, "n1");
    int uring_fd = open(str1, O_WRONLY);
    if (uring_fd < 0)
    {
        fprintf(stderr, "Failed to Open uring Device %s\n", str1);
        uring_fd = -1;
    }

    uint32_t key = atomic_fetch_add(&dev_handler_allocator, 1);

    dev_handler_register[key].admin_dev_handle = fd;
    dev_handler_register[key].compute_dev_handle = ret;
    dev_handler_register[key].uring_compute_dev_handle = uring_fd;
    if(uring_fd!=-1){
        dev_handler_register[key].ring_num = 1;
        dev_handler_register[key].ring = malloc(sizeof(struct io_uring));
        ret = pipe(dev_handler_register[key].pipe_fd);
        if(ret!=0)
        {
            fprintf(stderr,"Failed To allocate pipe\n");
            free(dev_handler_register[key].ring);
            return CS_INVALID_OPTION;
        }
        ret = pthread_create(&(dev_handler_register[key].async_thread),NULL,vscode_async_thread,&(dev_handler_register[key]));
        if(ret!=0){
            fprintf(stderr,"Failed To allocate async pthread\n");
            close(dev_handler_register[key].pipe_fd[0]);
            close(dev_handler_register[key].pipe_fd[1]);
            free(dev_handler_register[key].ring);
            return CS_INVALID_OPTION;
        }
    }
    fd = key;

    *DevHandle = fd;
    printf("Opened device : %s", DevName);
    return CS_SUCCESS;
}

CS_STATUS csCloseCSx(CS_DEV_HANDLE DevHandle)
{
    int k1, k2, k3;
    if (DevHandle < dev_handler_allocator)
    {
        return CS_INVALID_HANDLE;
    }
    else
    {
        k1 = dev_handler_register[DevHandle].admin_dev_handle;
        k2 = dev_handler_register[DevHandle].compute_dev_handle;
        k3 = dev_handler_register[DevHandle].uring_compute_dev_handle;
        if (k1 > 0)
            close(k1);
        if (k2 > 0)
            close(k2);
        if (k3 > 0)
            close(k3);
        return CS_SUCCESS;
    }
}

/**
 * @copydoc csGetCSEFromCSx
 * @todo This is still a stub
 * */
CS_STATUS csGetCSEFromCSx(CS_DEV_HANDLE DevHandle, unsigned int *Length,
                          char *CSEName)
{
    if (DevHandle < 0)
    {
        return CS_INVALID_ARG;
    }

    if (!Length)
    {
        return CS_INVALID_ARG;
    }

    if (*Length < strlen(__CS_PLACE_HOLDER_CSE_NAME) + 1)
    {
        return CS_INVALID_LENGTH;
    }
    else
    {
        strncpy(CSEName, __CS_PLACE_HOLDER_CSE_NAME, strlen(__CS_PLACE_HOLDER_CSE_NAME) + 1);
        printf("Returned CSE : %s", CSEName);
        return CS_SUCCESS;
    }
}

/**
 * @copydoc csOpenCSE
 * @todo this is still a stub
 * */
CS_STATUS csOpenCSE(char *CSEName, void *CSEContext,
                    CS_CSE_HANDLE *CSEHandle)
{
    /*
    if (strcmp(CSEName, __CS_PLACE_HOLDER_CSE_NAME))
    {
        return CS_ENTITY_NOT_ON_DEVICE;
    }

    *CSEHandle = 0x123;
    */
    return CS_SUCCESS;
}

/**
 * @copydoc csAllocMem
 * @todo this is still a stub
 * */
CS_STATUS csAllocMem(CS_DEV_HANDLE DevHandle, int Bytes, const CsMemFlags *MemFlags,
                     CS_MEM_HANDLE *MemHandle, CS_MEM_PTR *VAddressPtr)
{
    // Here we need to allocate memory on the CMB
    // If the Linux kernel/driver supports it, map it to userspace and assign VAddressPtr
    if (Bytes <= 0)
    {
        return CS_INVALID_ARG;
    }
    else if (Bytes > 4096 * 1024)
    {
        return CS_NOT_ENOUGH_MEMORY;
    }
    else if (MemFlags->Flags == CS_FDM_FILL)
    {
        return CS_INVALID_OPTION;
    }

    // Request memory on the CMB of the device
    if (!MemHandle)
    {
        return CS_INVALID_ARG;
    }

    PHYSICAL_ADDR phys_addr = NULL;
    int ret = nvme_create_slm_ns(dev_handler_register[DevHandle].admin_dev_handle, &phys_addr, Bytes);
    // The device will return a physical address
    if (ret != CS_SUCCESS)
    {
        return ret;
    }

    CS_MEM_HANDLE key = atomic_fetch_add(&mem_handler_allocator, 1);
    mem_handler_register[key].cs_dev_handle = DevHandle;
    mem_handler_register[key].mem_handle = phys_addr;
    *MemHandle = (CS_MEM_HANDLE)key;

    if (VAddressPtr)
    {
        return CS_COULD_NOT_MAP_MEMORY;
    }

    return CS_SUCCESS;
}

CS_STATUS csFreeMem(CS_MEM_HANDLE MemHandle, const CsMemFlags *MemFlags)
{

    int dev_id = dev_handler_register[mem_handler_register[MemHandle].cs_dev_handle].admin_dev_handle;
    int ret = nvme_delete_slm_ns(dev_id, mem_handler_register[MemHandle].mem_handle);
    if (ret != 0)
        return CS_UNKNOWN_MEMORY;
    return CS_SUCCESS;
}

CS_STATUS csInitMem(CS_MEM_HANDLE MemHandle, unsigned long ByteOffset,
                    int Bytes, const CsMemFlags *MemFlags)
{
    // TODO: Current Does not support
    return CS_INVALID_ARG;
}

CS_STATUS csQueueStorageRequest(const CsStorageRequest *Req, void *Context,
                                csQueueCallbackFn CallbackFn,
                                CS_EVT_HANDLE EventHandle,
                                CS_REQ_HANDLE *ReqHandle,
                                u64 *CompValue)
{
    if (!Req)
        return CS_INVALID_ARG;
    if (CallbackFn != NULL || EventHandle != NULL)
    {
        fprintf(stderr, "Current Does Not Support Async Event\n");
        return CS_INVALID_OPTION;
    }
    void *temp_data[4096] = {0};
    union nvme_source_range *sr = temp_data;
    int bdev_id, m_id;
    int ret = 0;
    int fid = 0;
    FILE *fp;
    struct stat file_stat;
    switch (Req->Mode)
    {
    case CS_STORAGE_BLOCK_IO:
        if (Req->u.BlockIo.NumRanges * sizeof(union nvme_source_range) > 4096)
            return CS_INVALID_OPTION;
        switch (Req->u.BlockIo.Type)
        {
        case CS_STORAGE_LOAD_TYPE:
            for (int i = 0; i < Req->u.BlockIo.NumRanges; i++)
            {
                sr[i].scc.snsid = Req->u.BlockIo.Range[i].NamespaceId;
                sr[i].scc.slba = Req->u.BlockIo.Range[i].StartLba;
                sr[i].scc.nlb = Req->u.BlockIo.Range[i].NumBlocks;
            }
            bdev_id = dev_handler_register[mem_handler_register[Req->u.BlockIo.DevMem.MemHandle].cs_dev_handle].compute_dev_handle;
            m_id = mem_handler_register[Req->u.BlockIo.DevMem.MemHandle].mem_handle;
            ret = nvme_slm_copy(bdev_id, temp_data, sizeof(union nvme_source_range) * Req->u.BlockIo.NumRanges,
                                Req->u.BlockIo.DevMem.ByteOffset, 0x3, Req->u.BlockIo.NumRanges, m_id);
            if (ret != 0)
            {
                fprintf(stderr, "Failed to Load Data\n");
                return CS_UNKNOWN_MEMORY;
            }
            break;
        case CS_STORAGE_STORE_TYPE:
            bdev_id = dev_handler_register[mem_handler_register[Req->u.BlockIo.DevMem.MemHandle].cs_dev_handle].compute_dev_handle;
            m_id = mem_handler_register[Req->u.BlockIo.DevMem.MemHandle].mem_handle;
            struct nvme_copy_args copy_args;
            copy_args.args_size = sizeof_args(struct nvme_copy_args, format, __u64);
            copy_args.fd = bdev_id;
            copy_args.nr = 1;
            copy_args.result = NULL;
            copy_args.nsid = 1; // NVM Namespace change it in the future TODO:)
            copy_args.copy = temp_data;
            copy_args.format = 4;
            unsigned long long cur_off = 0;
            for (int i = 0; i < Req->u.BlockIo.NumRanges; i++)
            {
                copy_args.nsid = Req->u.BlockIo.Range[i].NamespaceId;
                copy_args.sdlba = Req->u.BlockIo.Range[i].StartLba;
                sr[0].mc.saddr = Req->u.BlockIo.DevMem.ByteOffset + cur_off;
                cur_off += Req->u.BlockIo.Range[i].NumBlocks * 4096;
                sr[0].mc.nbyte = Req->u.BlockIo.Range[i].NumBlocks * 4096;
                sr[0].mc.snsid = m_id;
                ret = nvme_copy(&copy_args);
                if (ret != 0)
                {
                    fprintf(stderr, "Failed to Load Data\n");
                    return CS_UNKNOWN_MEMORY;
                }
            }
            break;
        default:
            return CS_INVALID_OPTION;
            break;
        }
        break;
    case CS_STORAGE_FILE_IO:
        /* code */
        fp = Req->u.FileIo.FileHandle;
        fid = fileno(fp);
        if (fid < 0 || fstat(fid, &file_stat) != 0 || !S_ISREG(file_stat.st_mode))
        {
            return CS_INVALID_OPTION;
        }
        char devname[PATH_MAX];
        if (major(file_stat.st_dev) == 0)
        {
            // 通过/proc/self/fd/[fd]获取文件路径
            char fd_path[64];
            snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fid);
            ssize_t len = readlink(fd_path, devname, sizeof(devname) - 1);
            if (len < 0)
            {
                return CS_INVALID_OPTION;
            }
            devname[len] = '\0';
            // 通过/proc/mounts查找文件系统挂载点对应的设备
            FILE *mtab = setmntent("/proc/mounts", "r");
            if (!mtab)
                return CS_INVALID_OPTION;
            struct mntent* entry;
            while((entry = getmntent(mtab))!=NULL){
                if(strncmp(entry->mnt_type,"ext",3)!=0)
                    continue;
                struct stat mnt_stat;
                if(stat(entry->mnt_dir,&mnt_stat)!=0)
                    continue;
                if(mnt_stat.st_dev == file_stat.st_dev){
                    strncpy(devname,entry->mnt_fsname,PATH_MAX-1);
                    endmntent(mtab);
                    break;
                }
            }
            endmntent(mtab);
        }
        if(strncmp(devname,"nvmq",4)!=0){
            return CS_INVALID_OPTION;
        }
        //TODO Check DEV PATH EQUAL TO FILE DEV PATH
        void* fiemapraw[4096];
        struct fiemap*fiemap;
        fiemap = fiemapraw;
        memset(fiemap,0,sizeof(fiemap)+sizeof(struct fiemap_extent)*FIEMAP_MAX_EXTENTS);
        fiemap->fm_start = Req->u.FileIo.Offset;
        fiemap->fm_length = Req->u.FileIo.Bytes;
        fiemap->fm_extent_count = FIEMAP_MAX_EXTENTS;
        ret = ioctl(fid,FS_IOC_FIEMAP,fiemap);
        if(ret < 0)
            return CS_INVALID_OPTION;
        
        switch (Req->u.FileIo.Type)
        {
         case CS_STORAGE_LOAD_TYPE:
            for (int i = 0; i < fiemap->fm_mapped_extents; i++)
            {
                struct fiemap_extent *extent = &fiemap->fm_extents[i];
                uint64_t lba = extent->fe_physical / 4096;
                uint32_t nlb = extent->fe_length / 4096;
                sr[i].scc.snsid = 1;//TODO Support more nsid in the future :>
                sr[i].scc.slba = lba;
                sr[i].scc.nlb = nlb;
            }
            bdev_id = dev_handler_register[mem_handler_register[Req->u.FileIo.DevMem.MemHandle].cs_dev_handle].compute_dev_handle;
            m_id = mem_handler_register[Req->u.FileIo.DevMem.MemHandle].mem_handle;
            ret = nvme_slm_copy(bdev_id, temp_data, sizeof(union nvme_source_range) *  fiemap->fm_mapped_extents,
                                Req->u.FileIo.DevMem.ByteOffset, 0x3,  fiemap->fm_mapped_extents, m_id);
            if (ret != 0)
            {
                fprintf(stderr, "Failed to Load Data\n");
                return CS_UNKNOWN_MEMORY;
            }
            break;
        case CS_STORAGE_STORE_TYPE:
            bdev_id = dev_handler_register[mem_handler_register[Req->u.FileIo.DevMem.MemHandle].cs_dev_handle].compute_dev_handle;
            m_id = mem_handler_register[Req->u.FileIo.DevMem.MemHandle].mem_handle;
            struct nvme_copy_args copy_args;
            copy_args.args_size = sizeof_args(struct nvme_copy_args, format, __u64);
            copy_args.fd = bdev_id;
            copy_args.nr = 1;
            copy_args.result = NULL;
            copy_args.nsid = 1; // NVM Namespace change it in the future TODO:)
            copy_args.copy = temp_data;
            copy_args.format = 4;
            unsigned long long cur_off = 0;
            for (int i = 0; i < fiemap->fm_mapped_extents; i++)
            {
                struct fiemap_extent *extent = &fiemap->fm_extents[i];
                uint64_t lba = extent->fe_physical / 4096;
                uint32_t nlb = extent->fe_length / 4096;
                copy_args.nsid = 1;
                copy_args.sdlba = lba;
                sr[0].mc.saddr = Req->u.BlockIo.DevMem.ByteOffset + cur_off;
                cur_off += nlb * 4096;
                sr[0].mc.nbyte = nlb * 4096;
                sr[0].mc.snsid = m_id;
                
                ret = nvme_copy(&copy_args);
                if (ret != 0)
                {
                    fprintf(stderr, "Failed to Load Data\n");
                    return CS_UNKNOWN_MEMORY;
                }
            }
        }
        break;
    default:
        return CS_INVALID_OPTION;
    }
}

CS_STATUS csQueueCopyMemRequest(const CsCopyMemRequest *CopyReq, void *Context,
                                       csQueueCallbackFn CallbackFn,
                                       CS_EVT_HANDLE EventHandle,
                                       CS_REQ_HANDLE *ReqHandle,
                                       u64 *CompValue){
    if (!CopyReq)
        return CS_INVALID_ARG;
    if (CallbackFn != NULL || EventHandle != NULL)
    {
        fprintf(stderr, "Current Does Not Support Async Event\n");
        return CS_INVALID_OPTION;
    }
    int dev_handle_sel = CopyReq->DevMem.MemHandle;
    int bdev_id = dev_handler_register[mem_handler_register[dev_handle_sel].cs_dev_handle].compute_dev_handle;
    int m_id = mem_handler_register[dev_handle_sel].mem_handle;
    int ret = 0;
    switch(CopyReq->Type){
        CS_COPY_TO_DEVICE:
            ret = nvme_slm_write(bdev_id,m_id,CopyReq->DevMem.ByteOffset,CopyReq->Bytes,(void*)CopyReq->HostVAddress);
            break;
        CS_COPY_FROM_DEVICE:
            ret = nvme_slm_read(bdev_id,m_id,CopyReq->DevMem.ByteOffset,CopyReq->Bytes,CopyReq->HostVAddress);
            break;
        default:
            return CS_INVALID_ARG;
    }
    if (ret != 0)
    {
        fprintf(stderr, "Failed to Copy Data\n");
        return CS_UNKNOWN_MEMORY;
    }
    return CS_SUCCESS;
}

/**
 * @copydoc csQueueComputeRequest
 * @todo this is still a stub
 * */
CS_STATUS csQueueComputeRequest(CsComputeRequest *Req, void *Context,
                                csQueueCallbackFn CallbackFn,
                                CS_EVT_HANDLE EventHandle,
                                u64 *CompValue)
{
    if (!Req)
        return CS_INVALID_ARG;
    if (EventHandle != NULL)
    {
        fprintf(stderr, "Current Does Not Support Async Event\n");
        return CS_INVALID_OPTION;
    }
    if(Req->NumArgs<3)
        return CS_INVALID_ARG;
    int dev_handle_sel = Req->Args[0].u.Value64; 
    int bdev_id = dev_handler_register[dev_handle_sel].compute_dev_handle;
    if(Req->FunctionId==VENDOR_DEFINED_MEMRANGE_ID){
        //Create Memory Range Set
        union memory_range_set_decriptor* mdes = (union memory_range_set_decriptor*)Req->Args[1].u.Value64;
        
        int ret = nvme_create_memory_range_set(bdev_id,2,CompValue,Req->Args[2].u.Value64,mdes);
        if(ret != 0){
            printf("Error Failed to create memory range set!\n");
            return CS_ERROR_IN_EXECUTION;
        }
    }else{
        if(Req->NumArgs<5)
            return CS_INVALID_ARG;
        struct AccContext* context = (struct AccContext*)Req->Args[1].u.Value64;
        u64 context_num = Req->Args[2].u.Value64;
        u64 rsid = Req->Args[3].u.Value64;
        u64 priority = Req->Args[4].u.Value64;
        if(CallbackFn==NULL){
            int ret = nvme_execute_hlsacc_program(bdev_id,2,rsid,Req->FunctionId,context,context_num,0,priority,CompValue);
            if(ret != 0){
                printf("Error Failed to execute program\n");
                return CS_ERROR_IN_EXECUTION;
            }
        }else{
            if(Context!=NULL&& dev_handler_register[dev_handle_sel].pipe_fd[1]>0){
                struct uring_cmd_callback_context write_data;
                write_data.cmd_num = 1;
                write_data.context = Context;
                struct nvme_uring_cmd *cmd = malloc(sizeof(struct nvme_uring_cmd));
                struct iovec *iovecs;
                write_data.CallbackFn = CallbackFn;//异步回调函数
                write_data.cmd = cmd;
                write_data.iovecs = iovecs;
                iovecs = calloc(1,sizeof(struct iovec));
                iovecs[0].iov_base = cmd;
                iovecs[0].iov_len = sizeof(struct nvme_uring_cmd);
                unsigned long long capram1,capram2,capram3;
                cmd->addr = (unsigned long long)context;//上下文地址数组
                cmd->nsid = 2;
                cmd->opcode = 1;
                cmd->data_len = sizeof(struct AccContext)*context_num;//有几个上下文需要初始化
                cmd->cdw2 = rsid << 16 | Req->FunctionId;//FunctionID为需要调用的程序ID RSID描述了数据的来源和目的
                cmd->cdw3 = 0;
                cmd->flags = 0;
                cmd->metadata_len = 0;
                for(int i=0;i<context_num;i++){
                    capram1 = ((capram1 << 8)|1);
                }
                capram2 = priority;
                cmd->cdw10 = (unsigned int)capram1;
                cmd->cdw11 = (unsigned int)(capram1 >> 32);
                cmd->cdw12 = (unsigned int)capram2;
                cmd->cdw13 = (unsigned int)(capram2 >> 32);
                cmd->cdw14 = (unsigned int)capram3;
                cmd->cdw15 = (unsigned int)(capram3 >> 32);
                struct io_uring_sqe *sqe;
                sqe = io_uring_get_sqe(&(dev_handler_register[dev_handle_sel].ring[0]));
                if(!sqe)
                    return CS_INVALID_ARG;
                io_uring_prep_writev(sqe,dev_handler_register[dev_handle_sel].uring_compute_dev_handle,
                iovecs,1,0);
                write(dev_handler_register[dev_handle_sel].pipe_fd[1],&write_data,sizeof(struct uring_cmd_callback_context));
            }else 
                return CS_INVALID_ARG;
        }
    }
    return CS_SUCCESS;
}

/**
 * @copydoc csHelperSetComputeArg
 * @todo this is still a stub
 * */
void csHelperSetComputeArg(CsComputeArg *ArgPtr,
                           CS_COMPUTE_ARG_TYPE Type, ...)
{
    if (ArgPtr)
    {
        ArgPtr->Type = Type;

        va_list args;
        va_start(args, Type);

        switch (Type)
        {
        case CS_AFDM_TYPE:
            ArgPtr->u.DevMem.MemHandle = va_arg(args, CS_MEM_HANDLE);
            ArgPtr->u.DevMem.ByteOffset = va_arg(args, unsigned long);
            printf("Setting argument of type AFDM, pointer : 0x%016lx, with byte offset : %lu",
                   (u64)ArgPtr->u.DevMem.MemHandle,
                   ArgPtr->u.DevMem.ByteOffset);
            break;
        case CS_32BIT_VALUE_TYPE:
            ArgPtr->u.Value32 = va_arg(args, u32);
            printf("Setting argument of type u32 with value : %u",
                   ArgPtr->u.Value32);
            break;
        case CS_64BIT_VALUE_TYPE:
            ArgPtr->u.Value64 = va_arg(args, u64);
            printf("Setting argument of type u64 with value : %lu",
                   ArgPtr->u.Value64);
            break;
        default:
            printf("Unsupported type");
            /// @todo
            break;
        }

        va_end(args);
    }
}

/**
 * @copydoc csQueryDeviceProperties
 * @TODO
 * */
CS_STATUS csQueryDeviceProperties(CS_DEV_HANDLE DevHandle, int *Length,
                                  CSxProperties *Buffer)
{
    return CS_SUCCESS;
}

/**
 * @copydoc csQueryDeviceCapabilities
 * */
CS_STATUS csQueryDeviceCapabilities(CS_DEV_HANDLE DevHandle,
                                    CsCapabilities *Caps)
{
    if (DevHandle < 0)
    {
        return CS_INVALID_HANDLE;
    }

    if (!Caps)
    {
        return CS_INVALID_ARG;
    }

    return CS_SUCCESS;
}

CS_STATUS csCSFDownload(CS_DEV_HANDLE DevHandle,
    const CsCSFDownloadInfo *Info,
    u32 *CSFId
){
    if(DevHandle < dev_handler_allocator){
        return CS_INVALID_HANDLE;
    }
    int dev_id = dev_handler_register[DevHandle].admin_dev_handle;

    if(!(Info->common.DataBuffer)){
        return CS_INVALID_ARG;
    }
    int ret = nvme_load_hlsacc_program(dev_id,Info->common.Length,Info->Index,2,Info->common.DataBuffer);
    if(ret < 0){
        return CS_LOAD_ERROR;
    }
    return CS_SUCCESS;
}