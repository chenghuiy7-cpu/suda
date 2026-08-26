#include <libnvme.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define LBA_SIZE 4096

static uint64_t parse_u64_arg(const char* s, uint64_t defv) {
    if (!s || !*s) return defv;
    char* end = NULL;
    unsigned long long v = strtoull(s, &end, 0);
    if (end == s) return defv;
    return (uint64_t)v;
}

int main(int argc, char** argv) {
    const char* admin_dev = "nvmq0";
    const char* io_dev = "nvmq0n1";

    uint32_t compute_nsid = 2;
    uint32_t src_nsid = 1;

    uint64_t slba = 0;
    uint32_t tfhe_mul_operator_type_id = 1000;
    uint32_t program_id = 10;

    if (argc > 1) admin_dev = argv[1];
    if (argc > 2) io_dev = argv[2];
    if (argc > 3) slba = parse_u64_arg(argv[3], 0);
    if (argc > 4) tfhe_mul_operator_type_id = (uint32_t)parse_u64_arg(argv[4], tfhe_mul_operator_type_id);
    if (argc > 5) program_id = (uint32_t)parse_u64_arg(argv[5], program_id);

    int admin_fd = nvme_open(admin_dev);
    int io_fd = nvme_open(io_dev);

    if (admin_fd < 0 || io_fd < 0) {
        fprintf(stderr, "Failed to open nvme devices: admin=%s io=%s\n", admin_dev, io_dev);
        return 1;
    }

    uint64_t input_buf_size = (uint64_t)LBA_SIZE * 256;
    uint64_t output_buf_size = (uint64_t)LBA_SIZE * 256;

    unsigned int input_mem_id = 0;
    unsigned int output_mem_id = 0;

    int ret = nvme_create_slm_ns(admin_fd, &input_mem_id, input_buf_size);
    if (ret != 0) {
        fprintf(stderr, "nvme_create_slm_ns(input) failed: %d\n", ret);
        return 1;
    }

    ret = nvme_create_slm_ns(admin_fd, &output_mem_id, output_buf_size);
    if (ret != 0) {
        fprintf(stderr, "nvme_create_slm_ns(output) failed: %d\n", ret);
        return 1;
    }

    union memory_range_set_decriptor mdes[2];
    memset(mdes, 0, sizeof(mdes));

    mdes[0].payload.mnsid = input_mem_id;
    mdes[0].payload.length = input_buf_size;
    mdes[0].payload.starting_byte = 0;
    mdes[0].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM;

    mdes[1].payload.mnsid = output_mem_id;
    mdes[1].payload.length = output_buf_size;
    mdes[1].payload.starting_byte = 0;
    mdes[1].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM;

    unsigned int rsid = 0;
    ret = nvme_create_memory_range_set(admin_fd, 2, &rsid, 2, mdes);
    if (ret != 0) {
        fprintf(stderr, "nvme_create_memory_range_set failed: %d\n", ret);
        return 1;
    }

    struct hlsacccompute_program p;
    memset(&p, 0, sizeof(p));

    p.input_channum = 1;
    p.output_channum = 1;
    p.program_id = program_id;

    p.apply_operators_id_map[0] = (uint8_t)tfhe_mul_operator_type_id;

    p.applyops[0].header.cid = 0;
    p.applyops[0].header.opc = APPLY_OPS;
    p.applyops[0].header.ops_num = 1;

    p.applyops[2].apply_ops_payload2.connections_num = 1;
    p.applyops[2].apply_ops_payload2.connections[0].from = 0 << 4 | 0;
    p.applyops[2].apply_ops_payload2.connections[0].to = 0xf0;

    p.pauseops[0].header.cid = 0;
    p.pauseops[0].header.opc = SUSPEND_OPS;
    p.pauseops[0].header.ops_num = 1;
    p.pauseops[1].generic_ops_payload.op_lists[0] = 0;

    p.freeops[0].header.cid = 0;
    p.freeops[0].header.opc = FORCE_FREE_OPS;
    p.freeops[0].header.ops_num = 1;
    p.freeops[1].generic_ops_payload.op_lists[0] = 0;

    p.input_channel_destination[0] = 0;
    p.apply_ops_size = 3;
    p.apply_operators_num = 1;
    p.esti_executed_time = 100;
    p.max_responded_time = p.esti_executed_time * 3;

    ret = nvme_load_hlsacc_program(admin_fd, (int)sizeof(struct hlsacccompute_program), program_id, compute_nsid, &p);
    if (ret != 0) {
        fprintf(stderr, "nvme_load_hlsacc_program failed: %d\n", ret);
        return 1;
    }

    ret = nvme_activate_program(admin_fd, program_id, compute_nsid);
    if (ret != 0) {
        fprintf(stderr, "nvme_activate_program failed: %d\n", ret);
        return 1;
    }

    union nvme_source_range* sr = (union nvme_source_range*)calloc(8, sizeof(union nvme_source_range));
    if (!sr) return 1;

    for (int j = 0; j < 8; j++) {
        sr[j].scc.slba = slba + (uint64_t)(j * 32);
        sr[j].scc.nlb = 31;
        sr[j].scc.snsid = src_nsid;
    }

    ret = nvme_slm_copy(io_fd, sr, (unsigned long long)(sizeof(union nvme_source_range) * 8), 0, 0x3, 8, input_mem_id);
    free(sr);
    if (ret != 0) {
        fprintf(stderr, "nvme_slm_copy failed: %d\n", ret);
        return 1;
    }

    struct timeval start, end;
    gettimeofday(&start, NULL);

    unsigned int exec_res = 0;
    ret = nvme_execute_hlsacc_program(io_fd, compute_nsid, rsid, program_id, NULL, 0, 0, 0, &exec_res);
    if (ret != 0) {
        fprintf(stderr, "nvme_execute_hlsacc_program failed: %d\n", ret);
        return 1;
    }

    gettimeofday(&end, NULL);
    double sec = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_usec - start.tv_usec) / 1000000.0;

    void* out = NULL;
    if (posix_memalign(&out, 4096, output_buf_size) != 0 || !out) {
        fprintf(stderr, "posix_memalign failed\n");
        return 1;
    }
    memset(out, 0, output_buf_size);

    ret = nvme_slm_read(io_fd, output_mem_id, 0, (int)output_buf_size, out);
    if (ret != 0) {
        fprintf(stderr, "nvme_slm_read failed: %d\n", ret);
        free(out);
        return 1;
    }

    printf("TFHE-MUL exec_res=%u time=%f s\n", exec_res, sec);
    printf("output[0..16]=");
    for (int i = 0; i < 16; i++) {
        printf("%02x", ((unsigned char*)out)[i]);
    }
    printf("\n");

    free(out);
    return 0;
}