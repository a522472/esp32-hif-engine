#include "py/dynruntime.h"

#define GPIO_OUT_W1TS_REG 0x60004008
#define GPIO_OUT_W1TC_REG 0x6000400C
#define GPIO_IN_REG       0x6000403C

static inline uint32_t get_ccount(void) {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0, ccount" : "=r"(ccount));
    return ccount;
}

// 接收 5 個參數：out_buf, in_buf, offset, cycle_ticks, stb_ticks
static mp_obj_t hif_engine_play(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t out_bufinfo;
    mp_buffer_info_t in_bufinfo;
    
    mp_get_buffer_raise(args[0], &out_bufinfo, MP_BUFFER_READ);
    mp_get_buffer_raise(args[1], &in_bufinfo, MP_BUFFER_WRITE);
    
    uint32_t cap_idx     = mp_obj_get_int(args[2]);
    uint32_t cycle_ticks = mp_obj_get_int(args[3]);
    uint32_t stb_ticks   = mp_obj_get_int(args[4]);

    // 因為 out_buf 是 [set, clr] 交替，所以長度要除以 2 才是總 Cycle 數
    uint32_t total_cycles = (out_bufinfo.len / sizeof(uint32_t)) / 2;
    uint32_t *out_data = (uint32_t *)out_bufinfo.buf;
    
    uint32_t in_length = in_bufinfo.len / sizeof(uint32_t);
    uint32_t *in_data = (uint32_t *)in_bufinfo.buf;

    volatile uint32_t *gpio_w1ts = (volatile uint32_t *)GPIO_OUT_W1TS_REG;
    volatile uint32_t *gpio_w1tc = (volatile uint32_t *)GPIO_OUT_W1TC_REG;
    volatile uint32_t *gpio_in   = (volatile uint32_t *)GPIO_IN_REG;

    uint32_t out_idx = 0;
    
    // 建立絕對時間軸
    uint32_t next_cycle_time = get_ccount();

    // 🚀 暴力的等時展開迴圈 (Constant-Time Loop)
    for (uint32_t i = 0; i < total_cycles; i++) {
        uint32_t set_m = out_data[out_idx++];
        uint32_t clr_m = out_data[out_idx++];

        *gpio_w1tc = clr_m;
        *gpio_w1ts = set_m;

        uint32_t target_stb = next_cycle_time + stb_ticks;
        while ((int32_t)(get_ccount() - target_stb) < 0) { }

        // 採樣並寫入記憶體 (加上防越界保護)
        if (cap_idx < in_length) {
            in_data[cap_idx] = *gpio_in;
        }
        cap_idx++;

        next_cycle_time += cycle_ticks;
        while ((int32_t)(get_ccount() - next_cycle_time) < 0) { }
    }
    
    return mp_obj_new_int(cap_idx);
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(hif_engine_play_obj, 5, 5, hif_engine_play);

mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    MP_DYNRUNTIME_INIT_ENTRY
    mp_store_global(MP_QSTR_play, MP_OBJ_FROM_PTR(&hif_engine_play_obj));
    MP_DYNRUNTIME_INIT_EXIT
}
