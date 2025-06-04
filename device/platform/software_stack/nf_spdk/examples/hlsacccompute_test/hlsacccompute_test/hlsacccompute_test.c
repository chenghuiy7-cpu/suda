#include "spdk/hlsacccompute.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk_internal/event.h"
#include "spdk/rpc.h"
#include "spdk/jsonrpc.h"
#include "spdk/hash_table.h"
#include "spdk/likely.h"
#include "spdk/axi_dma.h"
#include "dlfcn.h"
#include "sys/mman.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "unistd.h"

static bool test_app_fin_code = false;
unsigned long long *in_data;
unsigned long long *out_data;
double start_time;
static void compute_start(void *arg);
struct spdk_hlsacccompute_virtual_object ob, ob1;


struct test_context {
  enum {
    BEGIN_TWO_EXPR,
    MCDMA_STOP_TEST,
    MCDMA_STOP_TEST_STAGE2,
    CIRCLE_PREEMPT,
    TEST1_PROGRAM_CREATE_AND_DOWNLOAD,
    TEST1_2_APPLY_HASH_TABLE,
    TEST1_3_EXECUTE_SOFTWARE,
    TEST2_CHANNEL_CREATE_AND_SEND_DATA,
    TEST3_REQUEST_CREATE,
    TEST4_SQE_REQUEST_CREATE,
    TEST5_1_MULTI_PREEMPT_REQUESTS_CREATE,
    TEST5_RR_SCHEDULER_CREATE,
    TEST6_UPD_SCHEDULER_CREATE
  } fsm_state;
  struct spdk_hlsacccompute_dev *dev;
  int recv_count;
  struct spdk_hlsacccompute_channel *ch, *ch1;
  int req_count;
  void* tx_memory_buffer;
  void* rx_memory_buffer;
  struct spdk_hlsacccompute_program* program;
  uint64_t start_time;
  uint64_t request1_end_time;
  uint64_t request2_end_time;
  void* request1_address;
  void* request2_address;
  int test5_fin_count;
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
	/*
	if(wait_for_time){
	  SPDK_DEBUGLOG(hlsacc,"WAITTIMES\n");
	  //usleep(500);
	  usleep(data_left/PAGE_SIZE);
	}*/
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
		//int next_send_data =
		//	data_left > 64 * PAGE_SIZE ? 64 * PAGE_SIZE : data_left;
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
  


static void create_axi_dma_channel(int count, struct spdk_hlsacccompute_dev *dev, int phy_id_begin,struct spdk_axi_dma_dev* mcdma_dev)
{
  // TAILQ_INIT(&(dev->rx_channel_pool));
  // TAILQ_INIT(&(dev->tx_channel_pool));
  //static struct spdk_axi_dma_dev* mcdma_dev = NULL;
  struct spdk_hlsacccompute_channel *ch = spdk_malloc(sizeof(struct spdk_hlsacccompute_channel) * count * 2, 2, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
  struct axi_dma_channel_info *info = spdk_malloc(sizeof(struct axi_dma_channel_info) * count * 2, 2, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
  struct spdk_axi_dma_ch *axi_dma_ch = spdk_malloc(sizeof(struct spdk_axi_dma_ch) * count * 2, 2, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
  const char *g_mcdma_dev = "b0000000.dma";
  if(mcdma_dev==NULL)
    mcdma_dev = spdk_axi_dma_get_device(g_mcdma_dev);
  int i = 0;
  for (; i < count * 2; i++)
  {
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
    if (i >= count)
    {
      ch[i].is_tx = true;
      axi_dma_ch[i].env_ch = spdk_env_axi_dma_create_tx_channel(mcdma_dev->env_dev, 65, phy_id_begin + i - count, phy_id_begin + i - count);
      if (!axi_dma_ch[i].env_ch)
      {
        SPDK_ERRLOG("Failed To Create TX Channels\n");
        goto release_channel;
      }
      axi_dma_ch[i].id = phy_id_begin + i - count;
      ch[i].channel_id = phy_id_begin + i - count;
      // 创建iopool
      if ((spdk_simple_pool_init(&((axi_dma_ch[i]).io_pool), 4096, sizeof(struct spdk_axi_dma_io))) != 0)
      {
        SPDK_ERRLOG("Failed To Allocate AXI DMA IO POOL FOR CHANNEL %d\n", i);
        goto release_channel;
      }
      ch[i].channel_done = tx_channel_done;
      ch[i].channel_send = tx_channel_send;
      ch[i].channel_recv = tx_channel_recv;
      spdk_hlsacccompute_register_channel(dev, &ch[i]);
    }
    else
    {
      ch[i].is_tx = false;
      axi_dma_ch[i].env_ch = spdk_env_axi_dma_create_rx_channel(mcdma_dev->env_dev, 65, phy_id_begin + i, phy_id_begin + i);
      if (!axi_dma_ch[i].env_ch)
      {
        SPDK_ERRLOG("Failed To Create RX Channels\n");
        goto release_channel;
      }
      axi_dma_ch[i].id = phy_id_begin + i;
      ch[i].channel_id = phy_id_begin + i;
      ch[i].channel_done = rx_channel_done;
      ch[i].channel_send = rx_channel_send;
      ch[i].channel_recv = rx_channel_recv;
      // 创建iopool
      if ((spdk_simple_pool_init(&((axi_dma_ch[i]).io_pool), 4096, sizeof(struct spdk_axi_dma_io)) != 0))
      {
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





static int cq_poll(void *arg) {
  struct spdk_hlsacccompute_dev *dev = (struct spdk_hlsacccompute_dev *)arg;
  int ret = 0;
  uint32_t cqheader_incr_num = 0;
  bool has_cqe = false;
  while (dev->inner_ppair_cqtailer != *(dev->bar->ppair_cqtailer)) {
    uint32_t ppair_cqheader =
        (*(dev->bar->ppair_cqheader) + cqheader_incr_num) %
        SPDK_HLSACCCOMPUTE_CQ_SIZE;
    // if(dev->bar->ppair_cq[ppair_cqheader].header.cid==2)
    SPDK_NOTICELOG(
        "Operator Finish, cid%u,status %u Time Passed %.10lf\n",
        dev->bar->ppair_cq[ppair_cqheader].header.cid,
        dev->bar->ppair_cq[ppair_cqheader].header.state,
        ((((double)spdk_get_ticks()) / ((double)spdk_get_ticks_hz())) -
         (start_time)));

    cqheader_incr_num++;
    dev->inner_ppair_cqtailer =
        (dev->inner_ppair_cqtailer + 1) % SPDK_HLSACCCOMPUTE_SQ_SIZE;
    switch (dev->bar->ppair_cq[ppair_cqheader].header.cid) {
      case 255:
        // SPDK_NOTICELOG("RECV FIN CQ\n");
        break;
      case 1:
        // 如果是APPLY_OPS
        // 那就开始分配
        // SPDK_NOTICELOG("SEND_DATA");
        break;
      case 2:
        printf("used time %lld %.10lf\n", spdk_get_ticks(),
               ((((double)spdk_get_ticks()) / ((double)spdk_get_ticks_hz())) -
                (start_time)));
        break;
      default:
        break;
    }
    has_cqe = true;
  }
  if (cqheader_incr_num != 0) {
    (*(dev->bar->ppair_cqheader)) =
        (*(dev->bar->ppair_cqheader) + cqheader_incr_num) %
        SPDK_HLSACCCOMPUTE_SQ_SIZE;
  }
  return ret;
}



void hlsacccompute_req_callback(struct spdk_hlsacccompute_request *request,
                                void *cb_arg) {
  // 回调函数，根据当前上下文的状态去确定下一步
  switch (ctx.fsm_state) {
    case TEST2_CHANNEL_CREATE_AND_SEND_DATA:
      SPDK_NOTICELOG("OK TEST2 FINISH!\n");
      ctx.fsm_state = TEST3_REQUEST_CREATE;
      compute_start(NULL);
      break;
    case TEST3_REQUEST_CREATE:
      SPDK_NOTICELOG("OK TEST3 FINISH!\n");
      ctx.fsm_state = TEST4_SQE_REQUEST_CREATE;
      spdk_hlsacccompute_free_request(request->dev, request, true);
      compute_start(NULL);
      break;
    case TEST4_SQE_REQUEST_CREATE:
      spdk_hlsacccompute_free_request(request->dev,request,true);
      compute_start(NULL);
      break;
    case TEST5_1_MULTI_PREEMPT_REQUESTS_CREATE:
      if((void*)request == ctx.request1_address){
        ctx.request1_end_time = spdk_get_ticks();
        ctx.test5_fin_count++;
        spdk_hlsacccompute_free_request(request->dev,request,true);
      }else if((void*)request == ctx.request2_address){
        ctx.request2_end_time = spdk_get_ticks();
        ctx.test5_fin_count++;
        spdk_hlsacccompute_free_request(request->dev,request,true);
      }else{
        SPDK_ERRLOG("failed to run test5,undefined request\n");
      }
      if(ctx.test5_fin_count==2){
              // 计算执行时间（秒）
          double exec_time1 = (double)(ctx.request1_end_time - ctx.start_time) / spdk_get_ticks_hz();
          double exec_time2 = (double)(ctx.request2_end_time - ctx.start_time) / spdk_get_ticks_hz();
          // 打印执行时间，精确到三位小数
          printf("request1 execution time: %.3f seconds\n", exec_time1);
          printf("request2 execution time: %.3f seconds\n", exec_time2);
      }
      
      break;
    default:
      break;
  }
}

static void compute_start(void *arg) {
  static struct spdk_hlsacccompute_program *program1;
  struct spdk_thread *thread;
  int ret;
  uint32_t state = (uint32_t)ctx.fsm_state;
  do {
    state = (uint32_t)ctx.fsm_state;
    switch (ctx.fsm_state) {
      case TEST1_PROGRAM_CREATE_AND_DOWNLOAD:
        ctx.dev = (struct spdk_hlsacccompute_dev *)calloc(
            1, sizeof(struct spdk_hlsacccompute_dev));
        ret = spdk_hlsacccompute_dev_init(ctx.dev,
                                          SPDK_HLSACCCOMPUTE_BAR_PHYS_ADDR);
        if (ret < 0) {
          fprintf(stderr, "Failed to initialize compute device\n");
          return;
        }
        SPDK_NOTICELOG("Finished Initialized, Begin to Load Program");
        struct spdk_hlsacccompute_program *program;
        ret = spdk_hlsacccompute_get_program_container(&program);
        if (ret < 0) {
          fprintf(stderr, "Failed to allocate program container\n");
          return;
        }
        program->input_channum = 1;
        program->output_channum = 1;
        program->program_id = 0;
        program->applyops[0].header.cid = 0;
        program->applyops[0].header.opc = APPLY_OPS;
        program->applyops[0].header.ops_num = 1;
        program->applyops[2].apply_ops_payload2.connections_num = 1;
        program->applyops[2].apply_ops_payload2.connections[0].from =
            1 << 4 | 0;
        // ffff表示是通道，请求分配一条入口通道
        program->applyops[2].apply_ops_payload2.connections[1].to = 0xf0 & 0;
        program->pauseops[0].header.cid = 0;
        program->pauseops[0].header.opc = SUSPEND_OPS;
        program->pauseops[0].header.ops_num = 1;
        program->pauseops[1].generic_ops_payload.op_lists[0] = 1;
        program->freeops[0].header.cid = 0;
        program->freeops[0].header.opc = FORCE_FREE_OPS;
        program->freeops[0].header.ops_num = 1;
        program->freeops[1].generic_ops_payload.op_lists[0] = 1;
        program->apply_ops_size = 3;
        program->apply_operators_num = 1;
        program->esti_executed_time = 35;
        program->max_responded_time = program->esti_executed_time * 3;
        ret = spdk_hlsacccompute_add_program(ctx.dev, program);
        SPDK_NOTICELOG("ALLOCATE PROGRAM id%d\n", ret);

        spdk_hlsacccompute_get_program_container(&program1);
        memcpy(program1, program, sizeof(struct spdk_hlsacccompute_program));
        spdk_hlsacccompute_del_program(ctx.dev, ret);
        ret = spdk_hlsacccompute_add_program(ctx.dev, program1);
        ctx.fsm_state = TEST1_2_APPLY_HASH_TABLE;
        // break;
      case TEST1_2_APPLY_HASH_TABLE: {
        void *table = spdk_cuckoo_table_create(
            spdk_env_get_socket_id(spdk_env_get_current_core()),
            sizeof(struct spdk_hlsacccompute_virtual_object));
        struct spdk_hlsacccompute_virtual_object ob[3];
        // Initialize
        ob[0].iov_base =
            spdk_zmalloc(PAGE_SIZE, PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY,
                         SPDK_MALLOC_SHARE);
        ob[0].iov_len = PAGE_SIZE;
        ob[0].cur_used = 10;
        ob[0].is_mem = true;
        SPDK_NOTICELOG("ALLOC IOV BASE%lx CUR_USED %d\n", ob[0].iov_base,
                       ob[0].cur_used);
        // Add
        void *entry_ptr;  // Dont use
        spdk_cuckoo_table_entry_add(table, 32 << 16 | 1, &ob, &entry_ptr);
        // LookUp Expected Success
        struct spdk_hlsacccompute_virtual_object *entry;
        int ret = spdk_cuckoo_table_lookup(table, 32 << 16 | 1, &entry);

        if (entry != NULL) {
          SPDK_NOTICELOG("LOOKUP SUCCESS,\n");
          // LookUp Expected Failed
          entry = NULL;
          ret = spdk_cuckoo_table_lookup(table, 0, &entry);
          if (entry == NULL) {
            SPDK_NOTICELOG("LOOKUP SUCCESS\n");
            // Delete Table
            ret = spdk_cuckoo_table_entry_delete(table, 32 << 16 | 1, NULL);
            if (ret >= 0) {
              SPDK_NOTICELOG("DELERE ENTRY SUCCESS\n");
              entry = NULL;
              ret = spdk_cuckoo_table_lookup(table, 32 << 16 | 1, &entry);
              if (entry == NULL) {
                SPDK_NOTICELOG("LOOKUP SUCCESS\n");
                spdk_cuckoo_table_free(table);
                // Go To Next State

                // Allocate DEVMEM
                void *vaddr_ptr, *paddr_ptr;
                int id0 = spdk_hlsacccompute_devmem_malloc(
                    ctx.dev, PAGE_SIZE, PAGE_SIZE, &vaddr_ptr, &paddr_ptr);
                memset(vaddr_ptr, 'D', PAGE_SIZE);
                int id1 = spdk_hlsacccompute_devmem_malloc(
                    ctx.dev, PAGE_SIZE, PAGE_SIZE, &vaddr_ptr, &paddr_ptr);
                memset(vaddr_ptr, 'E', PAGE_SIZE);
                int id2 = spdk_hlsacccompute_devmem_malloc(
                    ctx.dev, PAGE_SIZE, PAGE_SIZE, &vaddr_ptr, &paddr_ptr);
                memset(vaddr_ptr, 'F', PAGE_SIZE);
                spdk_hlsacccompute_devmem_lookup(ctx.dev, id0, &vaddr_ptr,
                                                 &paddr_ptr);
                printf("EXPECTED A %c\n", ((char *)vaddr_ptr)[0]);
                spdk_hlsacccompute_devmem_lookup(ctx.dev, id1, &vaddr_ptr,
                                                 &paddr_ptr);
                printf("EXPECTED B %c\n", ((char *)vaddr_ptr)[0]);
                spdk_hlsacccompute_devmem_lookup(ctx.dev, id2, &vaddr_ptr,
                                                 &paddr_ptr);
                printf("EXPECTED C %c\n", ((char *)vaddr_ptr)[0]);
                ctx.fsm_state = TEST1_3_EXECUTE_SOFTWARE;
                break;
              } else {
                SPDK_NOTICELOG("LOOKUP FAILED\n");
                exit(-1);
              }
            } else {
              SPDK_NOTICELOG("LOOKUP FAILED\n");
              exit(-1);
            }
          } else {
            SPDK_NOTICELOG("LOOKUP FAILED\n");
            exit(-1);
          }
        } else {
          SPDK_NOTICELOG("LOOKUP FAILED\n");
          exit(-1);
        }
      } break;
      case TEST1_3_EXECUTE_SOFTWARE:
        //thread = spdk_thread_create("SW_COMPUTE", NULL);
        {
          FILE *fp;
          struct stat buf;

          fp = fopen("/root/lyh_nf_spdk/acc_lib/libplus.so.1.0.0", "r");
          if (fp == NULL) {
            SPDK_ERRLOG("Failed To load software file\n");
          } else {
            fstat(fileno(fp), &buf);
            // int size = fread(program1->software_data,buf.st_size,1,fp);
            int size = buf.st_size;
            if (size != buf.st_size) {
              SPDK_ERRLOG("Failed to read software data\n");
            } else {
              int shm_fd;
              void *handle = dlopen(
                  "/root/lyh_nf_spdk/acc_lib/libplus.so.1.0.0", RTLD_NOW);
              if (handle != NULL) {
                SPDK_NOTICELOG("Execute Write Command size%lx\n", size);
                shm_fd = shm_open("spdk_lib", O_RDWR | O_CREAT,
                                  S_IRUSR | S_IWUSR | S_IXUSR);
                if (shm_fd == -1) {
                  SPDK_ERRLOG("Failed to create shared memory\n");
                  return;
                }
                if (ftruncate(shm_fd, buf.st_size) != 0) {
                  SPDK_ERRLOG("Failed to create shared memory size\n");
                  return;
                }
                char *address;
                address = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                               shm_fd, 0);
                program1->software_data = address;
                if (address == MAP_FAILED) {
                  SPDK_ERRLOG("Failed to create software_data buffer\n");
                  return;
                }
                read(fileno(fp), program1->software_data, size);
                handle = dlopen("/dev/shm/spdk_lib", RTLD_NOW);
                if (handle == NULL) {
                  SPDK_ERRLOG("Failed to create handle\n");
                  return;
                }
              
                  SPDK_NOTICELOG("Finished\n");
                
                  //spdk_thread_destroy(thread);
                
              }
            }
          }
        }

        // return;
        ctx.fsm_state = TEST2_CHANNEL_CREATE_AND_SEND_DATA;
        break;
      case TEST2_CHANNEL_CREATE_AND_SEND_DATA:
        // 尝试初始化Channel池
        create_axi_dma_channel(4, ctx.dev, 4,NULL);
        // 请求一个RX通道
        struct spdk_hlsacccompute_channel *ch =
            spdk_hlsacccompute_apply_channel(ctx.dev, false);
        // 绑定一个计算请求
        struct spdk_hlsacccompute_request *req =
            spdk_hlsacccompute_create_request(ctx.dev, program1);
        ch->virtual_channel_id = 0;
        ch->dest_id = 0x4;
        // 不要忘记注册axi_dma_info
        //  请求一个虚拟对象
        // TAILQ_INIT(&(req->rx_vos[0]));
        struct spdk_hlsacccompute_virtual_object *ob =
            spdk_malloc(sizeof(struct spdk_hlsacccompute_virtual_object), 2,
                        NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        struct spdk_hlsacccompute_virtual_object *ob1 =
            spdk_malloc(sizeof(struct spdk_hlsacccompute_virtual_object), 2,
                        NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        ob->iov_base = spdk_zmalloc(PAGE_SIZE * 17, PAGE_SIZE, NULL,
                                    SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        ob->iov_len = PAGE_SIZE * 17;
        ob->cur_used = 0;
        ob->is_mem = true;
        // char indata = 'A';
        for (int i = 0; i < 17; i++) {
          for (int j = 0; j < 4096; j++) {
            ((char *)(ob->iov_base))[i * 4096 + j] = 'A' + i;
          }
        }
        TAILQ_INSERT_HEAD(&(req->rx_vos[0]), ob, link);
        // WARNING，不要忘记给ch分配req，注册req指针和virtual id
        ch->virtual_channel_id = 0;
        ch->req = req;
        // 触发接收数据请求
        //ch->channel_recv(ch);
        //sleep(1);
        // 暂停数据通道
        //ch->channel_pause(ch);
        //ch->channel_release(ch, false);
        // 尝试检查接收数据更新状态
        //SPDK_NOTICELOG("DATA RELEASE UPDATE%d\n", ob->cur_used);

        //ch->channel_apply(ch);
        // 重新尝试分配一下通道
        ch->channel_recv(ch);
        // 再创建一条发送通道
        SPDK_NOTICELOG("Create Compute Channel\n");
        struct spdk_hlsacccompute_channel *ch1 =
            spdk_hlsacccompute_apply_channel(ctx.dev, true);

        // 请求一个虚拟对象
        // TAILQ_INIT(&(req->tx_vos[0]));
        // struct spdk_hlsacccompute_virtual_object ob1;
        ob1->iov_base = spdk_zmalloc(PAGE_SIZE * 20, PAGE_SIZE, NULL,
                                     SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        ob1->iov_len = PAGE_SIZE * 16;
        ob1->cur_used = 0;
        ob1->is_mem = true;
        req->req_cb_fns = hlsacccompute_req_callback;
        for (int i = 0; i < 20; i++) {
          for (int j = 0; j < 4096; j++) {
            ((char *)(ob1->iov_base))[i * 4096 + j] = 'B' + i;
          }
        }
        if(!TAILQ_EMPTY(&(req->tx_vos[0]))){
          TAILQ_REMOVE(&(req->tx_vos[0]),TAILQ_FIRST(&(req->tx_vos[0])),link);
          SPDK_NOTICELOG("attempt to clear tx vos\n");
        }
        TAILQ_INSERT_HEAD(&(req->tx_vos[0]), ob1, link);
        // WARNING，不要忘记给ch分配req，注册req指针和virtual id
        ch1->virtual_channel_id = 0;
        ch1->req = req;
        ch1->dest_id = ch->channel_id;
        SPDK_NOTICELOG("TX CHANNEL ID%d,RX CHANNEL ID%d\n", ch1->channel_id,
                       ch->channel_id);
        // 触发发送数据请求
        ch1->channel_send(ch1);
        for(int i=0;i<64;i++);
        return;
        // 释放后，再次重新申请通道
        for(int m=0;m<2;m++){
          ch1->channel_pause(ch1);
          SPDK_NOTICELOG("Try to pause data\n");
          sleep(1);
          spdk_hlsacccompute_release_channel(ctx.dev, ch1);
          ch->channel_pause(ch);
          
          spdk_hlsacccompute_release_channel(ctx.dev, ch);
          ch = spdk_hlsacccompute_apply_channel(ctx.dev, false);
          ch->virtual_channel_id = 0;
          ch->dest_id = 0x4;
          req->req_cb_fns = hlsacccompute_req_callback;
          ch->req = req;
          
        
          if(!TAILQ_EMPTY(&(req->tx_vos[0]))){
            ch->channel_recv(ch);
            
            ch1 = spdk_hlsacccompute_apply_channel(ctx.dev, true);
            ctx.ch1 = ch1;
            ch1->virtual_channel_id = 0;
            ch1->req = req;
            ch1->req->tx_vos[0].tqh_first->iov_len = ch1->req->tx_vos[0].tqh_first->cur_used + PAGE_SIZE*3;
            ch1->dest_id = ch->channel_id;
            ch1->channel_send(ch1);

          }else{
            ctx.fsm_state = TEST3_REQUEST_CREATE;
            break;
          }
        }
        ctx.ch = ch;
        ctx.ch1 = ch1;
        break;
      case TEST3_REQUEST_CREATE:
        SPDK_NOTICELOG("OK,I COME TO STATE TEST3!\n");
        // 释放channel把
        if(ctx.ch1!=NULL)
          ctx.ch1->channel_pause(ctx.ch1);
        if(ctx.ch!=NULL)
          ctx.ch->channel_pause(ctx.ch);
        // spdk_hlsacccompute_release_channel(ctx.dev,ctx.ch);
        // spdk_hlsacccompute_release_channel(ctx.dev,ctx.ch1);
        if(ctx.ch)
        spdk_poller_pause(((struct axi_dma_channel_info*)(ctx.ch->channel_info))->ch->cmpl_poller);
        if(ctx.ch1)
        spdk_poller_pause(((struct axi_dma_channel_info*)(ctx.ch1->channel_info))->ch->cmpl_poller);
      
        sleep(2);
        // 创建一个新的Program
        ret = spdk_hlsacccompute_get_program_container(&program);
        if (ret < 0) {
          fprintf(stderr, "Failed to allocate program container\n");
          return;
        }
        program->input_channum = 1;
        program->output_channum = 1;
        program->program_id = 1;
        program->apply_operators_id_map[0] = 1;
        program->applyops[0].header.cid = 0;
        program->applyops[0].header.opc = APPLY_OPS;
        program->applyops[0].header.ops_num = 1;
        program->applyops[2].apply_ops_payload2.connections_num = 1;
        program->applyops[2].apply_ops_payload2.connections[0].from =
            0 << 4 | 0;
        // ffff表示是通道，请求分配一条出口通道
        program->applyops[2].apply_ops_payload2.connections[0].to = 0xf0;
        program->pauseops[0].header.cid = 0;
        program->pauseops[0].header.opc = SUSPEND_OPS;
        program->pauseops[0].header.ops_num = 1;
        program->pauseops[1].generic_ops_payload.op_lists[0] = 0;
        program->freeops[0].header.cid = 0;
        program->freeops[0].header.opc = FORCE_FREE_OPS;
        program->freeops[0].header.ops_num = 1;
        program->freeops[1].generic_ops_payload.op_lists[0] = 0;
        program->input_channel_destination[0] = 0;
        program->apply_ops_size = 3;
        program->apply_operators_num = 1;
        program->esti_executed_time = 35;
        program->max_responded_time = program->esti_executed_time * 3;
        ctx.program = program;
        ret = spdk_hlsacccompute_add_program_with_id(ctx.dev, program,2);
        if(ret == -1){
          SPDK_ERRLOG("Failed to load program\n");
          return;
        }
        struct spdk_hlsacccompute_request *request =
            spdk_hlsacccompute_create_request(ctx.dev, program);
        request->dev = ctx.dev;
        struct spdk_hlsacccompute_program *px = ctx.dev->program_list[2];
        //srequest->program = px;
        //ctx.dev->program_list[0] = px;
        request->priority = 122;
        request->req_cb_fns = hlsacccompute_req_callback;
        struct spdk_hlsacccompute_virtual_object *obx =
            TAILQ_FIRST(&(ctx.dev->vo_pool));
        obx->iov_base = spdk_zmalloc(PAGE_SIZE * 257, PAGE_SIZE, NULL,
                                     SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        obx->iov_len = PAGE_SIZE * 17;
        obx->cur_used = 0;
        obx->is_mem = true;
        TAILQ_REMOVE(&ctx.dev->vo_pool, obx, link);
        struct spdk_hlsacccompute_virtual_object *obk =
            TAILQ_FIRST(&(ctx.dev->vo_pool));
        TAILQ_REMOVE(&ctx.dev->vo_pool, obk, link);
        obk->iov_base = spdk_zmalloc(PAGE_SIZE * 256, PAGE_SIZE, NULL,
                                     SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
       
        obk->iov_len = PAGE_SIZE * 16;
        obk->cur_used = 0;
        obk->is_mem = true;
        TAILQ_INSERT_HEAD(&(request->tx_vos[0]), obk, link);
        TAILQ_INSERT_HEAD(&(request->rx_vos[0]), obx, link);
        SPDK_NOTICELOG("RUN REQUEST\n");
        spdk_hlsacccompute_run_request(ctx.dev, request, false);
        ctx.tx_memory_buffer = obk->iov_base;
        ctx.rx_memory_buffer = obx->iov_base;
        ctx.req_count = 0;
        ctx.fsm_state = TEST3_REQUEST_CREATE;
        //ctx.dev->poller = spdk_poller_register(spdk_hlsacccompute_poller,(void*)ctx.dev,0);
        break;
      case TEST4_SQE_REQUEST_CREATE:
        SPDK_NOTICELOG("GOTO TEST4\n");
        if (ctx.req_count >= 4) {
          SPDK_NOTICELOG("ALL OPERATE FINISH\n");
          ctx.fsm_state = TEST5_1_MULTI_PREEMPT_REQUESTS_CREATE;
          ctx.test5_fin_count = 0;
        } else {
          SPDK_NOTICELOG("iter:%d\n",ctx.req_count);
          struct spdk_hlsacccompute_program *program = ctx.dev->program_list[2];

          struct spdk_hlsacccompute_request *request =
              spdk_hlsacccompute_create_request(ctx.dev, program);
          request->dev = ctx.dev;
          request->priority = 122;
          request->req_cb_fns = hlsacccompute_req_callback;
          request->run_way = 0;
          struct spdk_hlsacccompute_virtual_object *obx =
              TAILQ_FIRST(&(ctx.dev->vo_pool));
          assert(!TAILQ_EMPTY(&(ctx.dev->vo_pool)));
          obx->iov_base = ctx.rx_memory_buffer;
          obx->iov_len = PAGE_SIZE * 64;
          obx->cur_used = 0;
          obx->is_mem = true;
          TAILQ_REMOVE(&ctx.dev->vo_pool, obx, link);
          struct spdk_hlsacccompute_virtual_object *obk =
              TAILQ_FIRST(&(ctx.dev->vo_pool));
          TAILQ_REMOVE(&ctx.dev->vo_pool, obk, link);
          obk->iov_base = ctx.tx_memory_buffer;
          obk->iov_len = PAGE_SIZE * 63;
          obk->cur_used = 0;
          obk->is_mem = true;
          SPDK_NOTICELOG("MEMORYBUFFER TX%llxRX%llx\n",ctx.tx_memory_buffer,ctx.rx_memory_buffer);
          TAILQ_INSERT_HEAD(&(request->tx_vos[0]), obk, link);
          TAILQ_INSERT_HEAD(&(request->rx_vos[0]), obx, link);
          int res = spdk_hlsacccompute_run_request(ctx.dev, request, false);
          assert(res==0);
          ctx.req_count++;
        }
        break;
      case TEST5_1_MULTI_PREEMPT_REQUESTS_CREATE:
        SPDK_NOTICELOG("OK I Come To Test5\n");
        struct spdk_hlsacccompute_program *px1;
        ret = spdk_hlsacccompute_get_program_container(&px1);
        if (ret < 0) {
          fprintf(stderr, "Failed to allocate program container\n");
          return;
        }
        ctx.start_time = spdk_get_ticks();
        px1->input_channum = 1;
        px1->output_channum = 1;
        px1->program_id = 3;
        px1->apply_operators_id_map[0] = 0;
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
       
        struct spdk_hlsacccompute_request *request_array[2];
        bool preempt_able = false;
        void* tx_space =  spdk_zmalloc(PAGE_SIZE * 256, PAGE_SIZE, NULL,
          SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        void* rx_space =  spdk_zmalloc(PAGE_SIZE * 257, PAGE_SIZE, NULL,
          SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        for (int i = 0; i < 2; i++) {
         
          request_array[i] =
              spdk_hlsacccompute_create_request(ctx.dev, px1);
              if(i==0)
              ctx.request1_address = (void*)(request_array[0]);
              else if(i==1)
              ctx.request2_address = (void*)(request_array[1]);
          request_array[i]->dev = ctx.dev;
          request_array[i]->priority = 65;
          request_array[i]->req_cb_fns = hlsacccompute_req_callback;
          struct spdk_hlsacccompute_virtual_object *obx =
              TAILQ_FIRST(&(ctx.dev->vo_pool));
          obx->iov_base = (uint64_t)tx_space;
          obx->iov_len = i==1?PAGE_SIZE * 17:PAGE_SIZE * 64;
          obx->cur_used = 0;
          obx->is_mem = true;
          TAILQ_REMOVE(&ctx.dev->vo_pool, obx, link);
          struct spdk_hlsacccompute_virtual_object *obk =
              TAILQ_FIRST(&(ctx.dev->vo_pool));
          TAILQ_REMOVE(&ctx.dev->vo_pool, obk, link);
          obk->iov_base = rx_space;
          obk->iov_len = i==1? PAGE_SIZE*16:PAGE_SIZE * 63;
          obk->cur_used = 0;
          obk->is_mem = true;
          TAILQ_INSERT_HEAD(&(request_array[i]->tx_vos[0]), obk, link);
          TAILQ_INSERT_HEAD(&(request_array[i]->rx_vos[0]), obx, link);
          int res =
              spdk_hlsacccompute_run_request(ctx.dev, request_array[i], preempt_able);
          preempt_able = ~preempt_able;  
         
        }
       
        ctx.request1_end_time = 0;
        ctx.request2_end_time = 0;
        
        break;
    }
    
  } while (state != ctx.fsm_state);
}

int main(int argc, char **argv) {
  struct spdk_app_opts opts = {};
  int rc = 0;

  /* Set default values in opts structure. */

  spdk_app_opts_init(&opts, sizeof(opts));
  opts.name = "hello_bdev";

  /*
   * spdk_app_start() will initialize the SPDK framework, call hello_start(),
   * and then block until spdk_app_stop() is called (or if an initialization
   * error occurs, spdk_app_start() will return with rc even without calling
   * hello_start().
   */
  ctx.fsm_state = TEST1_PROGRAM_CREATE_AND_DOWNLOAD;
  rc = spdk_app_start(&opts, compute_start, NULL);
  if (rc) {
    SPDK_ERRLOG("ERROR starting application\n");
  }

  spdk_app_fini();
  return rc;
}
SPDK_LOG_REGISTER_COMPONENT(testapp);