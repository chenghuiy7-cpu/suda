#include "spdk/stdinc.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/string.h"
#include "spdk/json.h"
#include "spdk/util.h"


// 定义操作符结构体
struct operator_info {
    char *operator_type;
    uint32_t operator_inport_num;
    uint32_t operator_outport_num;
    uint32_t validated_operate_time;
    uint32_t bram_size;
    TAILQ_ENTRY(operator_info) link;
};

// 定义操作符链表头
TAILQ_HEAD(operator_list, operator_info);
static struct operator_list g_operators;

// 全局配置结构体
struct app_config {
    struct operator_list *operators;
    bool is_initialized;
};

static struct app_config g_config = {
    .is_initialized = false
};

// JSON解析规则
static const struct spdk_json_object_decoder operator_decoders[] = {
    {"operator_type", offsetof(struct operator_info, operator_type), spdk_json_decode_string},
    {"operator_inport_num", offsetof(struct operator_info, operator_inport_num), spdk_json_decode_uint32},
    {"operator_outport_num", offsetof(struct operator_info, operator_outport_num), spdk_json_decode_uint32},
    {"validated_operate_time", offsetof(struct operator_info, validated_operate_time), spdk_json_decode_uint32},
    {"bram_size", offsetof(struct operator_info, bram_size), spdk_json_decode_uint32}
};

// 错误处理函数
static void
error_exit(const char *msg)
{
    SPDK_ERRLOG("%s\n", msg);
    spdk_app_stop(-1);
}

// 解析单个操作符
static int
parse_operator(struct spdk_json_val *val, struct operator_info **op)
{
    struct operator_info *new_op;
    int rc;

    new_op = calloc(1, sizeof(*new_op));
    if (new_op == NULL) {
        SPDK_ERRLOG("Failed to allocate operator info\n");
        return -ENOMEM;
    }

    rc = spdk_json_decode_object(val, operator_decoders,
                                SPDK_COUNTOF(operator_decoders),
                                new_op);
    if (rc < 0) {
        SPDK_ERRLOG("Failed to decode operator: %s\n", spdk_strerror(-rc));
        free(new_op);
        return rc;
    }

    *op = new_op;
    return 0;
}

// 解析操作符数组
static int
parse_operators_array(struct spdk_json_val *values)
{
    struct spdk_json_val *operator_val;
    struct operator_info *op;
    size_t array_size, i;
    int rc;

    array_size = values->len;
    operator_val = values + 1;

    for (i = 0; i < array_size; i++) {
        rc = parse_operator(operator_val, &op);
        if (rc < 0) {
            SPDK_ERRLOG("Failed to parse operator %zu\n", i);
            return rc;
        }

        TAILQ_INSERT_TAIL(&g_operators, op, link);

        operator_val = spdk_json_next(operator_val);
    }

    return 0;
}

// 加载配置文件
static int
load_config(void)
{
    FILE *f;
    char *buffer = NULL;
    long file_size;
    struct spdk_json_val *values = NULL;
    size_t values_cnt;
    struct spdk_json_val *operators_val;
    int rc = -1;

    // 读取配置文件
    f = fopen("/root/config.json", "r");
    if (!f) {
        SPDK_ERRLOG("Failed to open config file\n");
        return -1;
    }

    // 获取文件大小
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // 分配缓冲区
    buffer = malloc(file_size + 1);
    if (!buffer) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        fclose(f);
        return -1;
    }

    // 读取文件内容
    if (fread(buffer, 1, file_size, f) != (size_t)file_size) {
        SPDK_ERRLOG("Failed to read config file\n");
        goto cleanup;
    }
    buffer[file_size] = '\0';
    fclose(f);
    f = NULL;

    // 解析JSON
    rc = spdk_json_parse(buffer, file_size, NULL, 0, &values_cnt, SPDK_JSON_PARSE_FLAG_DECODE_IN_PLACE);
    if (rc < 0) {
        SPDK_ERRLOG("Failed to parse JSON config (phase 1)\n");
        goto cleanup;
    }

    values = calloc(values_cnt, sizeof(struct spdk_json_val));
    if (values == NULL) {
        SPDK_ERRLOG("Failed to allocate values array\n");
        rc = -ENOMEM;
        goto cleanup;
    }

    rc = spdk_json_parse(buffer, file_size, values, values_cnt, NULL, SPDK_JSON_PARSE_FLAG_DECODE_IN_PLACE);
    if (rc < 0) {
        SPDK_ERRLOG("Failed to parse JSON config (phase 2)\n");
        goto cleanup;
    }

    // 初始化操作符链表
    TAILQ_INIT(&g_operators);

    // 查找operators数组
    operators_val = spdk_json_object_first(values);
    while (operators_val != NULL) {
        if (spdk_json_strequal(operators_val, "operators") && 
            spdk_json_next(operators_val)->type == SPDK_JSON_VAL_ARRAY_BEGIN) {
            rc = parse_operators_array(spdk_json_next(operators_val));
            break;
        }
        operators_val = spdk_json_next(spdk_json_next(operators_val));
    }

    if (rc < 0) {
        SPDK_ERRLOG("Failed to parse operators array\n");
        goto cleanup;
    }

    g_config.operators = &g_operators;
    g_config.is_initialized = true;
    rc = 0;

cleanup:
    if (f) {
        fclose(f);
    }
    free(buffer);
    free(values);
    return rc;
}

// 清理资源
static void
cleanup_config(void)
{
    struct operator_info *op, *tmp;

    if (!g_config.is_initialized) {
        return;
    }

    TAILQ_FOREACH_SAFE(op, &g_operators, link, tmp) {
        TAILQ_REMOVE(&g_operators, op, link);
        free(op->operator_type);
        free(op);
    }

    g_config.is_initialized = false;
}

// 主应用程序回调
static void
app_start(void *arg)
{
    if (load_config() != 0) {
        error_exit("Failed to load configuration");
        return;
    }

    // 打印加载的配置信息
    struct operator_info *op;
    TAILQ_FOREACH(op, &g_operators, link) {
        SPDK_NOTICELOG("Operator: type=%s, in=%u, out=%u, time=%u, bram=%u\n",
                       op->operator_type,
                       op->operator_inport_num,
                       op->operator_outport_num,
                       op->validated_operate_time,
                       op->bram_size);
    }

    // 这里可以添加更多的处理逻辑

    spdk_app_stop(0);
}

// 停止回调
static void
app_shutdown(void)
{
    cleanup_config();
}

int
main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc;

    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "config_reader";
    opts.shutdown_cb = app_shutdown;

    rc = spdk_app_start(&opts, app_start, NULL);
    if (rc) {
        SPDK_ERRLOG("Application failed to start: %d\n", rc);
    }

    spdk_app_fini();
    return rc;
}