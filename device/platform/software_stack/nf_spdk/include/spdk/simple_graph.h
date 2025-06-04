#ifndef _SIMPLEGRAPH
#define _SIMPLEGRAPH
#include "spdk/log.h"
#include "spdk/stdinc.h"
#include "spdk/env.h"
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SPDK_SIMPLE_GRAPH_DEFAULT_SIDE_CAPACITY 5

typedef uintptr_t side_descriptor_ptr;

struct spdk_simple_graph_sides{
    uint8_t side_num;
    side_descriptor_ptr* side_list;
    uint8_t side_capacity;
};

struct spdk_simple_graph{
uint32_t vertex_num;
struct spdk_simple_graph_sides **graph_structure;
};



static void spdk_simple_graph_init(struct spdk_simple_graph* graph,uint32_t vertex_num){
    graph->vertex_num = vertex_num;
    graph->graph_structure = (struct spdk_simple_graph_sides**)spdk_zmalloc((vertex_num*sizeof(struct spdk_simple_graph_sides*)),2,NULL,SPDK_ENV_SOCKET_ID_ANY,SPDK_MALLOC_SHARE);
    for(int i=0;i<vertex_num;i++)
        graph->graph_structure[i] = (struct spdk_simple_graph_sides*)spdk_zmalloc((vertex_num*sizeof(struct spdk_simple_graph_sides)),2,NULL,SPDK_ENV_SOCKET_ID_ANY,SPDK_MALLOC_SHARE);
}

/**
 * 获取图节点的上游节点
 * lookup_vertex是查询的节点号
 * upstream_vertex是要返回的上游节点数组列表
 * upstream_vertex_side是上游节点数组列表的大小
 * 当上游节点数目大于这个大小，只会返回这个大小内的节点
 * 但是返回值包含所有上游节点的数目（单层）
*/
static uint32_t spdk_simple_graph_get_upstream_vertex(struct spdk_simple_graph* graph,uint32_t loopup_vertex,uint32_t* upstream_vertexs,uint32_t upstream_vertexs_size){
    if(loopup_vertex >= graph->vertex_num){
        SPDK_ERRLOG("Vertex_num is too big!!\n");
        return 0;
    }
    uint32_t index = 0;
    for(uint32_t i=0;i<graph->vertex_num;i++){
        if(graph->graph_structure[i][loopup_vertex].side_num != 0)
        {
            //表示有上游节点
            //返回其对应的边
            if(index <= upstream_vertexs_size){
                upstream_vertexs[index] = i; 
            }
            index++;
        }
    }
    return index;
}

/**
 * 获取图的边
 * src_vec和des_vec表示边的起始节点和目的节点
 * 返回值是边描述符指针
 * 边描述符指针结构不定，可能用于描述多条边
*/
static struct spdk_simple_graph_sides* spdk_simple_graph_get_sides(struct spdk_simple_graph* graph,uint32_t src_vec,uint32_t des_vec){
    if(src_vec >= graph->vertex_num || des_vec >= graph->vertex_num){
        SPDK_ERRLOG("Vertex_num is too big!!\n");
        return NULL;
    }
    return &graph->graph_structure[src_vec][des_vec];
}

/**
 * 更新图的边
 * 返回值-1表示出错
 * 返回值0表示正常
 * src_vec和des_vec表示边的起始节点和目的节点
 * upd_ptr是需要更新的边的信息
*/
static int spdk_simple_graph_update_sides(struct spdk_simple_graph* graph,uint32_t src_vec,uint32_t des_vec,struct spdk_simple_graph_sides upd_ptr){
    if(src_vec >= graph->vertex_num || des_vec >= graph->vertex_num){
        SPDK_ERRLOG("Vertex_num is too big!!\n");
        return -1;
    }
    graph->graph_structure[src_vec][des_vec] = upd_ptr;
    return 0;
}

/**
 * 新增一条图的边
 * 返回值-1表示出错
 * 返回值0表示正常
 * src_vec和des_vec表示边的起始节点和目的节点
 * rmv_index是需要更新的边的信息
 */
static int spdk_simple_graph_add_one_side(struct spdk_simple_graph* graph,uint32_t src_vec,uint32_t des_vec,side_descriptor_ptr side){
    if(graph->graph_structure[src_vec][des_vec].side_list == NULL){
        graph->graph_structure[src_vec][des_vec].side_capacity = SPDK_SIMPLE_GRAPH_DEFAULT_SIDE_CAPACITY;
        graph->graph_structure[src_vec][des_vec].side_list = spdk_zmalloc(sizeof(struct spdk_simple_graph_sides)*SPDK_SIMPLE_GRAPH_DEFAULT_SIDE_CAPACITY,2,NULL,SPDK_ENV_SOCKET_ID_ANY,SPDK_MALLOC_SHARE);
        graph->graph_structure[src_vec][des_vec].side_num = 1;
        graph->graph_structure[src_vec][des_vec].side_list[0] = side;
    } else {
        if(graph->graph_structure[src_vec][des_vec].side_num == graph->graph_structure[src_vec][des_vec].side_capacity){
            side_descriptor_ptr ptr = graph->graph_structure[src_vec][des_vec].side_list;
            graph->graph_structure[src_vec][des_vec].side_list  = spdk_zmalloc(sizeof(struct spdk_simple_graph_sides)*graph->graph_structure[src_vec][des_vec].side_capacity+SPDK_SIMPLE_GRAPH_DEFAULT_SIDE_CAPACITY,2,NULL,SPDK_ENV_SOCKET_ID_ANY,SPDK_MALLOC_SHARE);
            memcpy(graph->graph_structure[src_vec][des_vec].side_list,ptr,sizeof(struct spdk_simple_graph_sides)*graph->graph_structure[src_vec][des_vec].side_capacity);
            graph->graph_structure[src_vec][des_vec].side_capacity += SPDK_SIMPLE_GRAPH_DEFAULT_SIDE_CAPACITY;
            spdk_free(ptr);
        }
        graph->graph_structure[src_vec][des_vec].side_list[graph->graph_structure[src_vec][des_vec].side_num] = side;
        graph->graph_structure[src_vec][des_vec].side_num += 1;
    }
    return 0;
}

/**
 * 移除一条图的边
 * 返回值-1表示出错
 * 返回值0表示正常
 * src_vec和des_vec表示边的起始节点和目的节点
 * rmv_index是需要移除的边的索引
 */
static int spdk_simple_graph_remove_one_side(struct spdk_simple_graph* graph,uint32_t src_vec,uint32_t des_vec, uint32_t rmv_index){
     if(graph->graph_structure[src_vec][des_vec].side_list == NULL || rmv_index >= graph->graph_structure[src_vec][des_vec].side_num){
        SPDK_ERRLOG("Error! Side is NULL or Index is out of range!\n");
        return -1;
     }
     else{
        for(int i=rmv_index;i<graph->graph_structure[src_vec][des_vec].side_num-1;i++){
            graph->graph_structure[src_vec][des_vec].side_list[i] = graph->graph_structure[src_vec][des_vec].side_list[i+1];
        }
        graph->graph_structure[src_vec][des_vec].side_num --;
     }
     return 0;
}


#ifdef __cplusplus
}
#endif

#endif
