#include "py/dynruntime.h"

#define GPIO_OUT_W1TS_REG 0x60004008
#define GPIO_OUT_W1TC_REG 0x6000400C
#define GPIO_IN_REG       0x6000403C

// 👑 黑科技：直接讀取 ESP32 內部的絕對時脈計數器 (精準度 4.16ns)
static inline uint32_t get_ccount(void) {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0, ccount" : "=r"(ccount));
    return ccount;
}

static mp_obj_t hif_engine_play(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t out_bufinfo;
    mp_buffer_info_t in_bufinfo;
    
    mp_get_buffer_raise(args[0], &out_bufinfo, MP_BUFFER_READ);
    mp_get_buffer_raise(args[1], &in_bufinfo, MP_BUFFER_WRITE);
    
    // 現在 Python 傳進來的是「絕對 Ticks 數量」，而不是迴圈次數
    uint32_t cycle_ticks = mp_obj_get_int(args[2]);
    uint32_t stb_ticks = mp_obj_get_int(args[3]);

    size_t length = (out_bufinfo.len / sizeof(uint32_t)) / 2;
    uint32_t *out_data = (uint32_t *)out_bufinfo.buf;
    uint32_t *in_data = (uint32_t *)in_bufinfo.buf;

    volatile uint32_t *gpio_w1ts = (volatile uint32_t *)GPIO_OUT_W1TS_REG;
    volatile uint32_t *gpio_w1tc = (volatile uint32_t *)GPIO_OUT_W1TC_REG;
    volatile uint32_t *gpio_in   = (volatile uint32_t *)GPIO_IN_REG;

    size_t out_idx = 0;

    // 🚀 進入硬體領域
    for (size_t i = 0; i < length; i++) {
        // 1. 紀錄這個 CYCLE 開始的「絕對時間戳記」
        uint32_t start_tick = get_ccount();

        // 2. 記憶體讀取與 GPIO 輸出
        uint32_t set_m = out_data[out_idx++];
        uint32_t clr_m = out_data[out_idx++];
        *gpio_w1tc = clr_m;
        *gpio_w1ts = set_m;

        // 3. 盯著時鐘看，時間不到 STB 點絕對不往下走！(完美吸收所有記憶體延遲)
        while ((get_ccount() - start_tick) < stb_ticks) { }

        // 4. 精準採樣
        in_data[i] = *gpio_in;

        // 5. 盯著時鐘看，等待這個 CYCLE 完美結束
        while ((get_ccount() - start_tick) < cycle_ticks) { }
    }
    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(hif_engine_play_obj, 4, 4, hif_engine_play);

mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    MP_DYNRUNTIME_INIT_ENTRY
    mp_store_global(MP_QSTR_play, MP_OBJ_FROM_PTR(&hif_engine_play_obj));
    MP_DYNRUNTIME_INIT_EXIT
}
