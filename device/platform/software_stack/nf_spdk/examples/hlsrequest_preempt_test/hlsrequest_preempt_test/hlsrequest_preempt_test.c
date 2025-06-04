#include "spdk/hlsacccompute.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk_internal/event.h"
#include "spdk/rpc.h"
#include "spdk/jsonrpc.h"
#include "spdk/hash_table.h"
#include "spdk/likely.h"
#include "spdk/trace.h"
#include "spdk/axi_dma.h"
#include "dlfcn.h"
#include "sys/mman.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "unistd.h"
#include "spdk_internal/trace_defs.h"


SPDK_TRACE_REGISTER_FN(nvmf_trace, "nvmf_mcdma", TRACE_GROUP_NVMF_MCDMA)
{
	//spdk_trace_register_object(OBJECT_NVMF_RDMA_IO, 'r');

  //spdk_trace_register_description("MCDMA_HLS_EXEC",
  //  TRACE_MCDMA_REQUEST_STATE_HLS_EXEC,
  //  OWNER_NONE, OBJECT_NVMF_RDMA_IO, 0,
  //SPDK_TRACE_ARG_TYPE_INT, "etime");
	struct spdk_trace_tpoint_opts opts[] = {{
    "TASK_EXEC",TRACE_MCDMA_REQUEST_STATE_HLS_EXEC,OWNER_NONE,OBJECT_NONE,0,
      {
        {"time",SPDK_TRACE_ARG_TYPE_INT,8},
        {"id",SPDK_TRACE_ARG_TYPE_INT,8}
      }
  }
  };
  spdk_trace_register_description_ext(opts, 1);
}


static bool test_app_fin_code = false;
unsigned long long *in_data;
unsigned long long *out_data;
double start_time;
static void compute_start(void *arg);
struct spdk_hlsacccompute_virtual_object ob, ob1;


struct test_context {
  enum {
    NORM_TEST
  } fsm_state;
  struct spdk_hlsacccompute_dev *dev;
  void* req_addr[101];
  struct spdk_thread* request_submit_thread;
  struct spdk_thread* hlacc_thread;
  void* input_buffer[101];
  void* output_buffer[101];
  enum{
    BASIC_FCFS,
    FCFS,
    BASIC_PREEMPT,
    BASIC_SW_COWORK,
    PREEMPT_WITH_PRIORITY,
  }test_logic;
  unsigned int time_slice_counter;
  unsigned int total_try_count;
  unsigned int all_finished_task;
  unsigned int all_submited_task;
  pthread_mutex_t compute_mutex;
} ctx;


struct axi_dma_channel_info
{
	struct spdk_axi_dma_dev *dev;
	struct spdk_axi_dma_ch *ch;
	struct spdk_axi_dma_iovec iovs[64];
	unsigned char
		data_ptr;  // 指示当前iovs的指针，如果任务被暂停过，指针非0，否则为0（用于后续优化，未启用）
	unsigned char
		iovcnt;  // 指示当前使用了多少个iov--------------------------------（用于后续优化，未启用）
	bool is_working;
	bool is_last;  // 仅rx
				   // channel有用，表示收到了tuser=0xff的信号表示，数据接受完成
	bool reg_for_next_send;//确定下一次轮询是否要进行发送了
};



int tx_channel_send(struct spdk_hlsacccompute_channel *ch) {
	struct axi_dma_channel_info *axi_dma_info =
		(struct axi_dma_channel_info *)ch->channel_info;
	if (axi_dma_info == NULL || axi_dma_info->ch == NULL ||
		axi_dma_info->dev == NULL) {
	  SPDK_ERRLOG("Failed To Use Channel, Pointer is NULL\n");
	  return -1;
	}
	if (spdk_unlikely(axi_dma_info->ch->cmpl_poller == NULL)) {
	  axi_dma_info->ch->cmpl_poller =
		  spdk_poller_register(ch->channel_poller, ch, 0);
	}else{
	  spdk_poller_resume(axi_dma_info->ch->cmpl_poller);
	}
	struct spdk_axi_dma_ctrl ctrl;
	ctrl.tdest = ch->channel_id;
	ctrl.tid = ch->dest_id;
	ctrl.tuser = 0;
  SPDK_NOTICELOG("SEND TDEST%d TID%d\n",ctrl.tdest,ctrl.tid);
	axi_dma_info->is_working = true;
	if (spdk_unlikely(TAILQ_EMPTY(&ch->req->tx_vos[ch->virtual_channel_id]))) {
	  axi_dma_info->is_working = false;
	  ch->channel_done(ch);
	  return 0;
	}
	struct spdk_hlsacccompute_virtual_object *ob =
		TAILQ_FIRST(&ch->req->tx_vos[ch->virtual_channel_id]);
	if (spdk_unlikely(ob == NULL)) {  // 检查分配的ob地址是否为空，为空需要报错
	  SPDK_ERRLOG("VIRTUAL OBJECT PTR IS NULL\n");
	  return -1;
	}
	int size = ob->iov_len - ob->cur_used;
	int iovcnt = (size / PAGE_SIZE) + (size % PAGE_SIZE != 0);
	bool last_data = false;
	if (iovcnt > 48) {
	  iovcnt = 48;
	} else {
	  // 判断一下是否数据到达了Virtual Object链条的末尾
	  // 如果virtual
	  // object描述的数据大小超过了最大限制，就代表下一次还需要传输一部分数据，因此无需设置last_data
	  // 如果在最大限制内，而且到达末尾，代表数据last_data需要拉高，表示数据已经处理完成
	  if (TAILQ_NEXT(ob, link) == NULL) {
		last_data = true;
		SPDK_DEBUGLOG(hlsacc,"SEND LAST DATA ID%d\n",ch->req->request_id);
	  }
	}
	for (int i = 0; i < iovcnt; i++) {
	  axi_dma_info->iovs[i].iov_base =
		  ob->iov_base + ob->cur_used + PAGE_SIZE * i;
	  axi_dma_info->iovs[i].iov_len = PAGE_SIZE;
	  axi_dma_info->iovs[i].paddr =
		  spdk_vtophys(axi_dma_info->iovs[i].iov_base, NULL);
	}
	SPDK_DEBUGLOG(hlsacc,"CHANNEL SEND IOVCNT%d\n",iovcnt);
	axi_dma_info->iovcnt = iovcnt;
	spdk_axi_dma_tx_channel_send_seg(axi_dma_info->ch, axi_dma_info->iovs, iovcnt,
									 ch->channel_done, ch, &ctrl, last_data);
	return size;
  }
  int tx_channel_recv(struct spdk_hlsacccompute_channel *ch, void *dst, int size,
					  int align) {
	return -1;
  }
  int tx_channel_done(struct spdk_hlsacccompute_channel *ch) {
	if (!TAILQ_EMPTY(&(ch->req->tx_vos[ch->virtual_channel_id]))) {
	  // 如果virtual object通道不为空，那就继续发送
	  struct spdk_hlsacccompute_virtual_object *ob =
		  TAILQ_FIRST(&(ch->req->tx_vos[ch->virtual_channel_id]));
	  int data_left = ob->iov_len - ob->cur_used;
	  if (data_left == 0) {
		struct spdk_hlsacccompute_virtual_object *swap_ob = TAILQ_NEXT(ob, link);
		TAILQ_REMOVE(&(ch->req->tx_vos[ch->virtual_channel_id]), ob, link);
		//TODO Fix this bug in the future :(
		TAILQ_INSERT_HEAD(&(ch->req->dev->vo_pool),ob,link);
		//Get size
		struct spdk_hlsacccompute_virtual_object* obb;
		int ele = 0;
		TAILQ_FOREACH(obb,&(ch->req->dev->vo_pool),link){
		  ele++;
		}
		SPDK_DEBUGLOG(hlsacc,"DUMP VO_POOL SIZE%d\n",ele);
		ob = swap_ob;
		if (ob != NULL) data_left = ob->iov_len;
	  }
	  if (ob != NULL && data_left > 0 &&
		  ((struct axi_dma_channel_info *)(ch->channel_info))->is_working) {
		int next_send_data =
			data_left > 64 * PAGE_SIZE ? 64 * PAGE_SIZE : data_left;
		// data_left -= next_send_data;
		// 发送数据并不移动cur_used指针，只有数据发送完成，才更新cur_used指针
		((struct axi_dma_channel_info *)(ch->channel_info))->reg_for_next_send = true;
		//ch->channel_send(ch);
		return 0;
	  }else ((struct axi_dma_channel_info *)(ch->channel_info))->reg_for_next_send = false;
	}
	{
	  // 如果virtual object全部发送完成，此处可以进行最后的收尾工作
	  // 倘若request的类型是长服务类型，也就是数据源会随着时间进行动态更新
	  // 那就不通知释放，但是可以适度降低request的优先级
	  // 如果request不是上述类型，代表输入的数据已经全部完成，那可以选择释放通道资源
	  // 只有在rx_channel接收全部完成之后，才释放request资源
	  // tx_channel发送完毕只释放发送资源
	  // 此处偷懒，先选择不释放:)
	}
	return 0;
  }
  
  int tx_rx_channel_pause(struct spdk_hlsacccompute_channel *ch) {
	
	((struct axi_dma_channel_info *)(ch->channel_info))->is_working = false;
	if(((struct axi_dma_channel_info *)(ch->channel_info))->ch->cmpl_poller!=NULL)
	  spdk_poller_pause(((struct axi_dma_channel_info *)(ch->channel_info))->ch->cmpl_poller);
	  struct spdk_hlsacccompute_virtual_object *ob =
	  TAILQ_FIRST(&(ch->req->tx_vos[ch->virtual_channel_id]));
	bool wait_for_time = false;
	int data_left = 0;
	if(ob){
	  data_left = ob->iov_len - ob->cur_used;
	  if(data_left <= 64*PAGE_SIZE){
		wait_for_time = true;
	  }
	}
	if(wait_for_time){
	  SPDK_DEBUGLOG(hlsacc,"WAITTIMES\n");
	  //usleep(500);
	  usleep(data_left/PAGE_SIZE);
	}
	spdk_axi_dma_stop_channel(
		((struct axi_dma_channel_info *)(ch->channel_info))->ch);
	return 0;
  }
  
  int tx_rx_channel_release(struct spdk_hlsacccompute_channel *ch,
							bool context_save) {
	// 先暂停通道
	// spdk_axi_dma_stop_channel(((struct axi_dma_channel_info
	// *)(ch->channel_info))->ch); 然后统计一下当前发送出多少数据
	// 因为在poller里面，发送数据的信息已经被确认过了，所以可以直接保存资源并释放，但是为了保险起见
	// 还是需要重新统计一下
	((struct axi_dma_channel_info *)(ch->channel_info))->is_working = false;
	ch->channel_poller((void *)ch);
	
	spdk_simple_pool_reset(&(((struct axi_dma_channel_info *)(ch->channel_info))->ch->io_pool));							
	if(((struct axi_dma_channel_info *)(ch->channel_info))->ch->cmpl_poller!=NULL)
	  spdk_poller_pause(((struct axi_dma_channel_info *)(ch->channel_info))->ch->cmpl_poller);
	else{
		  assert(false);
	}
	
	//((struct axi_dma_channel_info *)(ch->channel_info))->ch->cmpl_poller = NULL;
	
	// 暂停Poller
	// 因为关键数据已经保存在context_save中，所以忽略上下文保存
	// SPDK_DEBUGLOG(hlsacc,"DATA PROCESSED%d\n",((struct axi_dma_channel_info
	// *)(ch->channel_info))->fin_used_data);
	int used_data = -1;
	if (ch->is_tx && !TAILQ_EMPTY(&(ch->req->tx_vos[ch->virtual_channel_id]))) {
	  used_data =
		  TAILQ_FIRST(&(ch->req->tx_vos[ch->virtual_channel_id]))->cur_used;
	} else if (!ch->is_tx &&
			   !TAILQ_EMPTY(&(ch->req->rx_vos[ch->virtual_channel_id]))) {
	  used_data =
		  TAILQ_FIRST(&(ch->req->rx_vos[ch->virtual_channel_id]))->cur_used;
	}
	if (used_data != -1) {
	  SPDK_DEBUGLOG(hlsacc,"DATA PROCESSED%d is_tx%d\n", used_data, ch->is_tx);
	} else {
	  SPDK_DEBUGLOG(hlsacc,"ALL DATA HAS BEEN SEND OUT\n");
	}
	((struct axi_dma_channel_info *)(ch->channel_info))->is_last = false;
	((struct axi_dma_channel_info *)(ch->channel_info))->iovcnt = 0;
	// 释放最新的io
  
	return 0;
  }
  
  int tx_rx_channel_poller(void *ctx)
  {
	struct spdk_hlsacccompute_channel *ch = (struct spdk_hlsacccompute_channel *)ctx;
	struct axi_dma_channel_info *axi_dma_info = (struct axi_dma_channel_info *)ch->channel_info;
	int transfered_length;
	struct spdk_axi_dma_io *io = spdk_axi_dma_poller_for_compute((((struct axi_dma_channel_info *)ch->channel_info))->ch);
	struct spdk_axi_dma_io *last_io = NULL;
	transfered_length = 0;
	if (io == NULL)
	  return 0;
	
	while (io)
	{
	  if (io->status.transfered_bytes == 0 || ((unsigned char)(io->ctrl.tuser)>>4) == 0xf)
	  {
		// 收到了tuser=0xff的信号，表示数据完全处理完成
		if (((!ch->is_tx) && ((unsigned char)(io->ctrl.tuser)>>4) == 0xf))
		{
		  SPDK_DEBUGLOG(hlsacc,"RECV REAL LAST SIGNAL\n");
		  axi_dma_info->is_last = true;
		  last_io = io;
		  spdk_axi_dma_io_free(io);	
		}
		break;
	  }
	  transfered_length += io->status.transfered_bytes;
	  last_io = io;
	  axi_dma_info->iovcnt--;
	  io = spdk_axi_dma_poller_for_compute((((struct axi_dma_channel_info *)ch->channel_info))->ch);

	  //这里axi_dma_io的分配需要重新设计和优化！！！
	  if(((last_io->used_iovcnt))<=0){
		spdk_axi_dma_io_free(last_io);
	  }
	}
	//注意！！！当存在多个通道的时候，这个设计会带来BUG！
	//TODO 后续修复这个BUG！！！
	if(!(ch->is_tx)){
		ch->req->result += transfered_length;
	}
	if (transfered_length == 0&&!(!ch->is_tx&&axi_dma_info->is_last)){
	  return 0;
	}
  
	if (ch->is_tx)
	{
  
	  struct spdk_hlsacccompute_virtual_object *ob = TAILQ_FIRST(&(ch->req->tx_vos[ch->virtual_channel_id]));
	  ob->cur_used += transfered_length;
	  SPDK_DEBUGLOG(hlsacc,"POLLED TX GET DATA%d CHANNEL ID%d\n", transfered_length,ch->channel_id);
	  SPDK_DEBUGLOG(hlsacc,"CUR USED%d\n",ob->cur_used);
	  if (ob->cur_used == ob->iov_len)
	  {
		TAILQ_REMOVE(&(ch->req->tx_vos[ch->virtual_channel_id]), ob, link);
		TAILQ_INSERT_HEAD(&(ch->req->dev->vo_pool),ob,link);
	  }
	  else if (spdk_unlikely(ob->cur_used > ob->iov_len))
	  {
		SPDK_ERRLOG("ERROR! DATA USED IS BIGGER THAN NEEDED!\n");
		return -1;
	  }
	}
	else
	{
	  struct spdk_hlsacccompute_virtual_object *ob = TAILQ_FIRST(&(ch->req->rx_vos[ch->virtual_channel_id]));
	  ob->cur_used += transfered_length;
	  SPDK_DEBUGLOG(hlsacc,"POLLED RX GET DATA%d CHANNEL ID%d\n", transfered_length,ch->channel_id);
	 
	  if (ob->cur_used >= ob->iov_len)
	  {
  
		TAILQ_REMOVE(&(ch->req->rx_vos[ch->virtual_channel_id]), ob, link);
		TAILQ_INSERT_HEAD(&(ch->req->dev->vo_pool),ob,link);
	  }
	  else if (spdk_unlikely(ob->cur_used > ob->iov_len))
	  {
		SPDK_ERRLOG("ERROR! DATA USED IS BIGGER THAN NEEDED!\n");
		return -1;
	  }
	}
	
  
	// 注意，如果传输完成所有数据，或者调用release函数触发了is_working变更为false，均会导致函数axi_dma_io结构体释放！
	

	if(((!ch->is_tx)&&axi_dma_info->iovcnt<=0)||axi_dma_info->is_working == false || axi_dma_info->is_last == true || ((ch->is_tx&&axi_dma_info->iovcnt==0))){
	  ch->channel_done(ch);
	}
	if(axi_dma_info->reg_for_next_send==true&&ch->is_tx&&axi_dma_info->is_working!=false){
	  ch->channel_send(ch);
	  axi_dma_info->reg_for_next_send = false;
	}
	return 0;
  }
  int tx_rx_channel_apply(struct spdk_hlsacccompute_channel *ch) {
	struct axi_dma_channel_info *axi_dma_info =
		(struct axi_dma_channel_info *)ch->channel_info;
	spdk_axi_dma_enable_channel(axi_dma_info->ch);
	return 0;
  }
  int rx_channel_send(struct spdk_hlsacccompute_channel *ch, void *src, int size,
					  int align) {
	return -1;
  }
  int rx_channel_recv(struct spdk_hlsacccompute_channel *ch) {
  //SPDK_NOTICELOG("RECV CHANNEL REQ ID%d\n",ch->req->request_id);
  
	struct axi_dma_channel_info *axi_dma_info =
		(struct axi_dma_channel_info *)ch->channel_info;
	if (axi_dma_info == NULL || axi_dma_info->ch == NULL ||
		axi_dma_info->dev == NULL) {
	  SPDK_ERRLOG("Failed To Use Channel, Pointer is NULL\n");
	  return -1;
	}
	if (spdk_unlikely(axi_dma_info->ch->cmpl_poller == NULL)) {
	  axi_dma_info->ch->cmpl_poller =
		  spdk_poller_register(ch->channel_poller, ch, 0);
	  
	}else{
	  spdk_poller_resume(axi_dma_info->ch->cmpl_poller);
	}
	axi_dma_info->is_working = true;
	struct spdk_hlsacccompute_virtual_object *ob =
		TAILQ_FIRST(&ch->req->rx_vos[ch->virtual_channel_id]);
	if (spdk_unlikely(ob == NULL)) {  // 检查分配的ob地址是否为空，为空需要报错
	  SPDK_ERRLOG("VIRTUAL OBJECT PTR IS NULL\n");
	  return -1;
	}
	int size = ob->iov_len - ob->cur_used;
	int iovcnt = (size / PAGE_SIZE) + (size % PAGE_SIZE != 0);
	int max_cnt = 64 - axi_dma_info->iovcnt;
	if (iovcnt > max_cnt) {
	  iovcnt = max_cnt;
	  // SPDK_ERRLOG("Failed To Use Channel, Send Data Size Too Big\n");
	  // return -1;
	}
	if(iovcnt==0) return 0;
	for (int i = 0; i < iovcnt; i++) {
	  axi_dma_info->iovs[i].iov_base =
		  ob->iov_base + ob->cur_used + PAGE_SIZE * i;
	  axi_dma_info->iovs[i].iov_len = PAGE_SIZE;
	  axi_dma_info->iovs[i].paddr =
		  spdk_vtophys(axi_dma_info->iovs[i].iov_base, NULL);
	}
	axi_dma_info->iovcnt = iovcnt;
	spdk_axi_dma_rx_channel_recv(axi_dma_info->ch, axi_dma_info->iovs, iovcnt,
								 ch->channel_done, (void *)ch);
	SPDK_DEBUGLOG(hlsacc,"CHANNEL RECV\n");
	return 0;
  }
  int rx_channel_done(struct spdk_hlsacccompute_channel *ch) {
	if (((struct axi_dma_channel_info *)(ch->channel_info))->is_last&&
	(ch->req->req_cb_fns!=NULL)) {
	  ch->req->req_cb_fns(ch->req, ch->req->req_cb_args);
	  return 0;
	}
	if (!TAILQ_EMPTY(&(ch->req->rx_vos[ch->virtual_channel_id]))) {
	  // 如果virtual object通道不为空，那就继续发送
	  struct spdk_hlsacccompute_virtual_object *ob =
		  TAILQ_FIRST(&(ch->req->rx_vos[ch->virtual_channel_id]));
	  int data_left = ob->iov_len - ob->cur_used;
	  if (data_left == 0) {
		struct spdk_hlsacccompute_virtual_object *swap_ob = TAILQ_NEXT(ob, link);
		TAILQ_REMOVE(&(ch->req->rx_vos[ch->virtual_channel_id]), ob, link);
		//TODO Fix this bug in the future :(
		TAILQ_INSERT_TAIL(&(ch->req->dev->vo_pool),ob,link);
		ob = swap_ob;
		if (ob != NULL) data_left = ob->iov_len;
	  }
	  //TODO FIX!!!!
	  if (ob != NULL && data_left > 0 &&
		  ((struct axi_dma_channel_info *)(ch->channel_info))->is_working&&!(((struct axi_dma_channel_info *)(ch->channel_info))->is_last)) {
		int next_send_data =
			data_left > 64 * PAGE_SIZE ? 64 * PAGE_SIZE : data_left;
		// data_left -= next_send_data;
		// 发送数据并不移动cur_used指针，只有数据发送完成，才更新cur_used指针
		ch->channel_recv(ch);
		return 0;
	  }
	}
	{
	  //TODO FIx!!
	  // 如果virtual object全部发送完成，此处可以进行最后的收尾工作
	  // 倘若request的类型是长服务类型，也就是数据源会随着时间进行动态更新
	  // 那就不通知释放，但是可以适度降低request的优先级
	  // 如果request不是上述类型，代表输入的数据已经全部完成，那可以选择释放通道资源
	  // 只有在rx_channel接收全部完成之后，才释放request资源
	  // tx_channel发送完毕只释放发送资源
	  if (ch->req->req_cb_fns != NULL&&((struct axi_dma_channel_info *)(ch->channel_info))->is_working)
		ch->req->req_cb_fns(ch->req, ch->req->req_cb_args);
	}
	return 0;
  }
  


static void create_axi_dma_channel(int count,
                                   struct spdk_hlsacccompute_dev *dev,
                                   int phy_id_begin) {
  static struct spdk_axi_dma_dev *mcdma_dev = NULL;
  struct spdk_hlsacccompute_channel *ch =
      spdk_malloc(sizeof(struct spdk_hlsacccompute_channel) * count * 2, 2,
                  NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
  struct axi_dma_channel_info *info =
      spdk_malloc(sizeof(struct axi_dma_channel_info) * count * 2, 2, NULL,
                  SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
  struct spdk_axi_dma_ch *axi_dma_ch =
      spdk_malloc(sizeof(struct spdk_axi_dma_ch) * count * 2, 2, NULL,
                  SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
  const char *g_mcdma_dev = "b0000000.dma";
  if (mcdma_dev == NULL) mcdma_dev = spdk_axi_dma_get_device(g_mcdma_dev);
  int i = 0;
  for (; i < count * 2; i++) {
    ch[i].channel_info = (void *)&(info[i]);
    info[i].ch = &(axi_dma_ch[i]);
    info[i].dev = mcdma_dev;
    info[i].iovcnt = 0;
    info[i].is_last = false;
    axi_dma_ch[i].cmpl_poller = NULL;
    axi_dma_ch[i].thread = spdk_get_thread();
    ch[i].channel_pause = tx_rx_channel_pause;
    ch[i].channel_poller = tx_rx_channel_poller;
    ch[i].channel_release = tx_rx_channel_release;
    ch[i].channel_apply = tx_rx_channel_apply;
    if (i >= count) {
      ch[i].is_tx = true;
      axi_dma_ch[i].env_ch = spdk_env_axi_dma_create_tx_channel(
          mcdma_dev->env_dev, 65, phy_id_begin + i - count,
          phy_id_begin + i - count);
      if (!axi_dma_ch[i].env_ch) {
        SPDK_ERRLOG("Failed To Create TX Channels\n");
        goto release_channel;
      }
      axi_dma_ch[i].id = phy_id_begin + i - count;
      ch[i].channel_id = phy_id_begin + i - count;
      // 创建iopool
      if ((spdk_simple_pool_init(&((axi_dma_ch[i]).io_pool), 65,
                                 sizeof(struct spdk_axi_dma_io))) != 0) {
        SPDK_ERRLOG("Failed To Allocate AXI DMA IO POOL FOR CHANNEL %d\n", i);
        goto release_channel;
      }
      ch[i].channel_done = tx_channel_done;
      ch[i].channel_send = tx_channel_send;
      ch[i].channel_recv = tx_channel_recv;
      spdk_hlsacccompute_register_channel(dev, &ch[i]);
    } else {
      ch[i].is_tx = false;
      axi_dma_ch[i].env_ch = spdk_env_axi_dma_create_rx_channel(
          mcdma_dev->env_dev, 65, phy_id_begin + i, phy_id_begin + i);
      if (!axi_dma_ch[i].env_ch) {
        SPDK_ERRLOG("Failed To Create RX Channels\n");
        goto release_channel;
      }
      axi_dma_ch[i].id = phy_id_begin + i;
      ch[i].channel_id = phy_id_begin + i;
      ch[i].channel_done = rx_channel_done;
      ch[i].channel_send = rx_channel_send;
      ch[i].channel_recv = rx_channel_recv;
      // 创建iopool
      if ((spdk_simple_pool_init(&((axi_dma_ch[i]).io_pool), 65,
                                 sizeof(struct spdk_axi_dma_io)) != 0)) {
        SPDK_ERRLOG("Failed To Allocate AXI DMA IO POOL FOR CHANNEL %d\n", i);
        goto release_channel;
      }
      spdk_hlsacccompute_register_channel(dev, &ch[i]);
    }
  }
operator_done:
  return;
release_channel:
  return;
}


void hlsacccompute_run_request_preempt(void* ctx){
	
	struct spdk_hlsacccompute_request* request = ctx;
  //spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_HLS_EXEC, 0, 0,
	//	NULL, "execp");
	spdk_hlsacccompute_run_request(request->dev,request,true);
}

void hlsacccompute_run_request_unpreempt(void* ctx){
	struct spdk_hlsacccompute_request* request = ctx;
	//spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_HLS_EXEC, 0, 0,
	//	NULL, "execip");
	spdk_hlsacccompute_run_request(request->dev,request,false);
}

void hlsacccompute_req_callback(struct spdk_hlsacccompute_request *request,
  void *cb_arg);

struct spdk_hlsacccompute_request* generate_request(int len,int priority){
  //每隔500us，发起一个抢占的任务
  struct spdk_hlsacccompute_program* px1;
  px1 = ctx.dev->program_list[3];
  assert(px1!=NULL);
  //SPDK_NOTICELOG("CREATE REQ AT CORE%d\n",spdk_env_get_current_core());
  pthread_mutex_lock(&(ctx.compute_mutex));
  struct spdk_hlsacccompute_request* req = spdk_hlsacccompute_create_request(ctx.dev,px1);
  assert(!TAILQ_EMPTY(&(ctx.dev->vo_pool)));
  struct spdk_hlsacccompute_virtual_object *tx_ob = TAILQ_FIRST(&(ctx.dev->vo_pool));
  TAILQ_REMOVE(&(ctx.dev->vo_pool),tx_ob,link);
  assert(!TAILQ_EMPTY(&(ctx.dev->vo_pool)));
  struct spdk_hlsacccompute_virtual_object *rx_ob = TAILQ_FIRST(&(ctx.dev->vo_pool));
  TAILQ_REMOVE(&(ctx.dev->vo_pool),rx_ob,link);
  pthread_mutex_unlock(&(ctx.compute_mutex));

  int rx_len = len+5;
  int tx_len = len;
  rx_ob->iov_base = ctx.output_buffer[0];
  rx_ob->iov_len = PAGE_SIZE * rx_len;
  rx_ob->cur_used = 0;
  rx_ob->is_mem = true;
  tx_ob->iov_base = ctx.input_buffer[0];
  tx_ob->iov_len = PAGE_SIZE * tx_len;
  tx_ob->cur_used = 0;
  tx_ob->is_mem = true;
  req->req_cb_fns = hlsacccompute_req_callback;
  
  req->priority = priority;
  TAILQ_INSERT_HEAD(&(req->tx_vos[0]), tx_ob, link);
  TAILQ_INSERT_HEAD(&(req->rx_vos[0]), rx_ob, link);
  return req;
}

int hlsacccompute_request_inject(void* context){
  //random inject
  
 
  switch(ctx.test_logic){
    case BASIC_FCFS:
      //一把梭哈，把100个任务一次性全部发出去
      break;
    case BASIC_PREEMPT:
    case FCFS:
    case BASIC_SW_COWORK:
     
      {
        struct spdk_hlsacccompute_request* req;
        if(ctx.time_slice_counter == 0&&ctx.all_submited_task==0){
          
          req = generate_request(256,0);
          SPDK_NOTICELOG("SUBMIT REQUEST ALL SUBMIT TASK%dREQ ADDRESS%llx\n",ctx.all_submited_task,req);
          assert(req!=NULL&&req->program!=NULL);
          uint64_t cur_time = spdk_get_ticks();
          spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_HLS_EXEC, 0, 0,
            0  , (int)0,req->request_id);
          spdk_thread_send_msg(ctx.hlacc_thread,hlsacccompute_run_request_unpreempt,req);
          req->tracker.responding_time = cur_time;
          ctx.all_submited_task++;
         
        }
        else if((ctx.time_slice_counter+1)%2 == 0&&ctx.all_submited_task<=1){
          req = generate_request(16,200);
          SPDK_NOTICELOG("SUBMIT REQUEST ALL SUBMIT TASK%dREQ ADDRESS%llx\n",ctx.all_submited_task,req);
          assert(req!=NULL&&req->program!=NULL);
          uint64_t cur_time = spdk_get_ticks();
          spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_HLS_EXEC, 0, 0,
            0  , (int)0,req->request_id);
          if(ctx.test_logic==FCFS||ctx.test_logic==BASIC_SW_COWORK)
            spdk_thread_send_msg(ctx.hlacc_thread,hlsacccompute_run_request_unpreempt,req);
          else
            spdk_thread_send_msg(ctx.hlacc_thread,hlsacccompute_run_request_preempt,req);
          
          req->tracker.responding_time = cur_time;
          ctx.all_submited_task++;
        }
        ctx.time_slice_counter = (ctx.time_slice_counter+1)%10;
        
    }
  }
}


void hlsacccompute_req_callback(struct spdk_hlsacccompute_request *request,
                                void *cb_arg) {
  int id = request->request_id;
  volatile uint64_t old_time = request->tracker.responding_time;
  
  //pthread_mutex_lock(&(ctx.compute_mutex));
  spdk_hlsacccompute_free_request(request->dev,request,true);
  int respond_time = spdk_get_ticks() - old_time;
  //pthread_mutex_unlock(&(ctx.compute_mutex));
  double elapsed_us = (double)respond_time * 1000 * 1000 / spdk_get_ticks_hz();
  spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_HLS_EXEC, 0, 0,
    0  , (int)elapsed_us,id);
  SPDK_NOTICELOG("TASK DONE\n");
  ctx.all_finished_task++;
  if(ctx.all_finished_task == 6){
    SPDK_NOTICELOG("ALL TASKED FINISH!\n");
  }
  // 回调函数，根据当前上下文的状态去确定下一步
  /*
  switch (ctx.fsm_state) {
  
    default:
      break;
  }
  if(request==ctx.req_addr[0]){
    SPDK_DEBUGLOG(hlsacc,"REQUEST 1 FINISHED\n");
    spdk_hlsacccompute_free_request(request->dev,request,true);
    //SPDK_DEBUGLOG(hlsacc,"SECOND PREEMPT");
  }else if(request==ctx.req_addr[1]){
    SPDK_DEBUGLOG(hlsacc,"REQUEST 2 FINISHED\n");
    spdk_hlsacccompute_free_request(request->dev,request,true);
    while(((struct spdk_hlsacccompute_request*)(ctx.req_addr[0]))->tracker.status!=ACC_REQ_EXECUTING){
      spdk_hlsacccompute_poll_cq(ctx.dev);
    }
    for(int i=0;i<64;i++);
    struct spdk_hlsacccompute_request* nextreq = ctx.req_addr[2];
    spdk_hlsacccompute_run_request(nextreq->dev,nextreq,true);
  }else if(request==ctx.req_addr[2]){
    SPDK_DEBUGLOG(hlsacc,"REQUEST 3 FINISHED\n");
    spdk_hlsacccompute_free_request(request->dev,request,true);
    
    while(((struct spdk_hlsacccompute_request*)(ctx.req_addr[0]))->tracker.status!=ACC_REQ_EXECUTING){
      spdk_hlsacccompute_poll_cq(ctx.dev);
    }
    for(int i=0;i<64;i++);
    struct spdk_hlsacccompute_request* nextreq = ctx.req_addr[3];
    spdk_hlsacccompute_run_request(nextreq->dev,nextreq,true);
  }else if(request==ctx.req_addr[3]){
    SPDK_DEBUGLOG(hlsacc,"REQUEST 4 FINISHED\n");
    spdk_hlsacccompute_free_request(request->dev,request,true);
  }
  else{
    
    SPDK_ERRLOG("FAILED TO MEET CONTEXT\n");
  }*/
}


void hlsacccompute_request_inject_begin(void* context)
{ 
  ctx.time_slice_counter = 0;
  ctx.total_try_count = 0;
  ctx.all_finished_task = 0;
  ctx.all_submited_task = 0;
  struct spdk_poller* poller;
  switch(ctx.test_logic){  
    //组织多个测试
    //第一个基础的，一次性发送大量的计算请求，看看能不能正常的排序执行
    //第二个抢占的，一次发送一个大的长计算请求（处理16MB数据），然后插入64KB的小请求，看看能不能正常执行
    //第三个抢占的，定期发送3000us长计算请求（1MB数据），然后正态分布插入64KB小请求，看看能不能正常执行
    case BASIC_FCFS:
      for(int i=1;i<4;i++)
      {
        spdk_thread_send_msg(ctx.hlacc_thread,hlsacccompute_run_request_unpreempt,ctx.req_addr[i]);        
      }
      break;
    default:
      poller = spdk_poller_register(hlsacccompute_request_inject,NULL,300);
      //poller = spdk_poller_register(hlsacccompute_request_inject,NULL,3000);
      assert(poller);
      break;
  }
}

static void compute_start(void *arg) {
  int ret;
  uint32_t state = (uint32_t)ctx.fsm_state;
  do {
    //SPDK_NOTICELOG("RUNNING\n");
    state = (uint32_t) ctx.fsm_state;
    struct spdk_cpuset* cpuset = spdk_cpuset_alloc();
    spdk_cpuset_set_cpu(cpuset,spdk_env_get_current_core(),1);
    
    ctx.hlacc_thread = spdk_get_thread();
    
    
    ctx.dev = spdk_malloc(sizeof(struct spdk_hlsacccompute_dev),4096,NULL,SPDK_ENV_SOCKET_ID_ANY,SPDK_MALLOC_SHARE);
    spdk_hlsacccompute_dev_init(ctx.dev,SPDK_HLSACCCOMPUTE_BAR_PHYS_ADDR);
    pthread_mutexattr_t		attr;
    if (pthread_mutexattr_init(&attr)) {
      SPDK_ERRLOG("COMPUTE DEV pthread_mutexattr_init() failed\n");
      return NULL;
    }

    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)) {
      SPDK_ERRLOG("COMPUTE DEV pthread_mutexattr_settype() failed\n");
      pthread_mutexattr_destroy(&attr);
      return NULL;
    }
    int phy_id_begin = 4;
	  pthread_mutex_init(&(ctx.compute_mutex),&attr);
    //ctx.dev->schedule_strategy = SCHED_BASIC_SOFTWARE_COCACULATE;
    ctx.dev->schedule_strategy = SCHED_BASIC_PRIORITY_PREEMPT;
    struct spdk_hlsacccompute_program *px1,*px2;
    ret = spdk_hlsacccompute_get_program_container(&px1);
    if (ret < 0) {
      fprintf(stderr, "Failed to allocate program container\n");
      return;
    }

    ret = spdk_hlsacccompute_get_program_container(&px2);
    if (ret < 0) {
      fprintf(stderr, "Failed to allocate program container\n");
      return;
    }
    px1->input_channum = 1;
    px1->output_channum = 1;
    px1->program_id = 3;
    px1->apply_operators_id_map[0] = 1;
    px1->applyops[0].header.cid = 0;
    px1->applyops[0].header.opc = APPLY_OPS;
    px1->applyops[0].header.ops_num = 1;
    px1->applyops[2].apply_ops_payload2.connections_num = 1;
    px1->applyops[2].apply_ops_payload2.connections[0].from =
        0 << 4 | 0;
    // ffff表示是通道，请求分配一条出口通道
    px1->applyops[2].apply_ops_payload2.connections[0].to = 0xf0;
    px1->pauseops[0].header.cid = 0;
    px1->pauseops[0].header.opc = SUSPEND_OPS;
    px1->pauseops[0].header.ops_num = 1;
    px1->pauseops[1].generic_ops_payload.op_lists[0] = 0;
    px1->freeops[0].header.cid = 0;
    px1->freeops[0].header.opc = FORCE_FREE_OPS;
    px1->freeops[0].header.ops_num = 1;
    px1->freeops[1].generic_ops_payload.op_lists[0] = 0;
    px1->input_channel_destination[0] = 0;
    px1->apply_ops_size = 3;
    px1->apply_operators_num = 1;
    px1->esti_executed_time = 35;
    px1->max_responded_time = px1->esti_executed_time * 3;
    ret = spdk_hlsacccompute_add_program_with_id(ctx.dev, px1,3);
   
    px2->input_channum = 1;
    px2->output_channum = 1;
    px2->program_id = 3;
    px2->apply_operators_id_map[0] = 0;
    px2->applyops[0].header.cid = 0;
    px2->applyops[0].header.opc = APPLY_OPS;
    px2->applyops[0].header.ops_num = 1;
    px2->applyops[2].apply_ops_payload2.connections_num = 1;
    px2->applyops[2].apply_ops_payload2.connections[0].from =
        0 << 4 | 0;
    // ffff表示是通道，请求分配一条出口通道
    px2->applyops[2].apply_ops_payload2.connections[0].to = 0xf0;
    px2->pauseops[0].header.cid = 0;
    px2->pauseops[0].header.opc = SUSPEND_OPS;
    px2->pauseops[0].header.ops_num = 1;
    px2->pauseops[1].generic_ops_payload.op_lists[0] = 0;
    px2->freeops[0].header.cid = 0;
    px2->freeops[0].header.opc = FORCE_FREE_OPS;
    px2->freeops[0].header.ops_num = 1;
    px2->freeops[1].generic_ops_payload.op_lists[0] = 0;
    px2->input_channel_destination[0] = 0;
    px2->apply_ops_size = 3;
    px2->apply_operators_num = 1;
    px2->esti_executed_time = 35;
    px2->max_responded_time = px1->esti_executed_time * 3;
    ret = spdk_hlsacccompute_add_program_with_id(ctx.dev, px2,2);
    FILE* fp = fopen("/root/lyh_nf_spdk/acc_lib/libblowfish.so.1.0.0","r");
    if(fp == NULL){
        fprintf(stderr,"Failed to load dynamic library\n");
        spdk_app_fini();
        return;
    }
    struct stat buf;
    fstat(fileno(fp),&buf);
    fseek(fp,0,SEEK_SET);
    //SPDK_NOTICELOG("SIZE%d\n", buf.st_size);
    int size = buf.st_size;
    spdk_hlsacccompute_add_program_sw_data(&px1,size);
    int read_size = read(fileno(fp),px1->software_data,size);
    assert(size == read_size);
    if(size!=read_size){
      SPDK_NOTICELOG("OH NO! read_size%d\n",read_size);
    }
    spdk_hlsacccompute_add_program_sw_data(&px1,0);
    spdk_hlsacccompute_add_program_sw_data(&px2,size);
    fseek(fp,0,SEEK_SET);
    read_size = read(fileno(fp),px2->software_data,size);
    assert(size == read_size);
    spdk_hlsacccompute_add_program_sw_data(&px2,0);
    create_axi_dma_channel(4,ctx.dev,4);
    for(int i=0;i<1;i++){
        struct spdk_hlsacccompute_request* req = spdk_hlsacccompute_create_request(ctx.dev,px1);
        ctx.req_addr[i] = req;
        struct spdk_hlsacccompute_virtual_object *tx_ob =
          spdk_malloc(sizeof(struct spdk_hlsacccompute_virtual_object), 2,
                      NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        struct spdk_hlsacccompute_virtual_object *rx_ob =
          spdk_malloc(sizeof(struct spdk_hlsacccompute_virtual_object), 2,
                      NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        int rx_len = i==0?(256+5):17;
        int tx_len = i==0?256:16;
        rx_ob->iov_base = spdk_zmalloc(PAGE_SIZE * rx_len, PAGE_SIZE, NULL,
                                  SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        rx_ob->iov_len = PAGE_SIZE * rx_len;
        rx_ob->cur_used = 0;
        rx_ob->is_mem = true;
        tx_ob->iov_base = spdk_zmalloc(PAGE_SIZE * tx_len, PAGE_SIZE, NULL,
          SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        tx_ob->iov_len = PAGE_SIZE * tx_len;
        tx_ob->cur_used = 0;
        tx_ob->is_mem = true;
        ctx.input_buffer[i] = tx_ob->iov_base;
        ctx.output_buffer[i] = rx_ob->iov_base;
        for(unsigned int m=0;m<tx_len;m++){
          unsigned long long* address = (unsigned long long*)(tx_ob->iov_base+PAGE_SIZE*m);
          for(unsigned long long n=0;n<PAGE_SIZE/sizeof(unsigned long long);n++){
            if(n%8==0)
            address[n] = m;
          }
        }
        req->req_cb_fns = hlsacccompute_req_callback;
        
        req->priority = i==0? 0:200;
        TAILQ_INSERT_HEAD(&(req->tx_vos[0]), tx_ob, link);
        TAILQ_INSERT_HEAD(&(req->rx_vos[0]), rx_ob, link);
        //spdk_hlsacccompute_run_request(req->dev,req,false);
        //return;
      
    }
    
    ctx.request_submit_thread = spdk_thread_create("SUBMITREQ",NULL);
    if(ctx.request_submit_thread==NULL){
      SPDK_ERRLOG("Failed to get request thread\n");
      spdk_app_fini();
    }
    spdk_thread_send_msg(ctx.request_submit_thread,hlsacccompute_request_inject_begin,cpuset);
    return;
    /*
    spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_HLS_EXEC, 0, 0,
      NULL, "run");
    spdk_hlsacccompute_run_request(ctx.dev,ctx.req_addr[0],false);
    while(((struct spdk_hlsacccompute_request*)(ctx.req_addr[0]))->tracker.status!=ACC_REQ_EXECUTING){
      spdk_hlsacccompute_poll_cq(ctx.dev);
    }

    spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_HLS_EXEC, 0, 0,
      NULL, "pollfn");
    */
   
    
  } while (state != ctx.fsm_state);
}


static void
basic_usage(void)
{
}

static int
basic_parse_arg(int ch, char *arg)
{
	return 0;
}

int main(int argc, char **argv) {
  struct spdk_app_opts opts = {};
  int rc = 0;

  /* Set default values in opts structure. */

  spdk_app_opts_init(&opts, sizeof(opts));
  opts.name = "basic";
  if ((rc = spdk_app_parse_args(argc, argv, &opts, "", NULL,
      basic_parse_arg, basic_usage)) !=
    SPDK_APP_PARSE_ARGS_SUCCESS) {
    exit(rc);
  }
  /*
   * spdk_app_start() will initialize the SPDK framework, call hello_start(),
   * and then block until spdk_app_stop() is called (or if an initialization
   * error occurs, spdk_app_start() will return with rc even without calling
   * hello_start().
   */
  ctx.test_logic = BASIC_PREEMPT;
  rc = spdk_app_start(&opts, compute_start, NULL);
  if (rc) {
    SPDK_ERRLOG("ERROR starting application\n");
  }

  spdk_app_fini();
  return rc;
}
SPDK_LOG_REGISTER_COMPONENT(testapp);