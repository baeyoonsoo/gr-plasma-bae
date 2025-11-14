/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_PLASMA_SPECTRO_SINK_IMPL_H
#define INCLUDED_PLASMA_SPECTRO_SINK_IMPL_H

#include "spectro_window.h"
#include <gnuradio/plasma/spectro_sink.h>
#include <gnuradio/high_res_timer.h>
#include <pmt/pmt.h>
#include <atomic>
#include <vector>
#include <queue> 
#include <cstdlib>
#include <stdexcept>
#include <sqlite3.h>

namespace gr {
namespace plasma {

class spectro_sink_impl : public spectro_sink
{
private:
    // private:
    uint64_t d_last_emit_ticks = 0;
    bool     d_have_emit_tick  = false;

    // Block parameters
    int d_fft_size;
    double d_samp_rate;
    size_t d_ncol;
    double d_center_freq;

    // GUI parameters
    SpectroWindow* d_main_gui;

    std::atomic<bool> d_finished;
    pmt::pmt_t d_in_port;
    size_t d_msg_queue_depth;

    pmt::pmt_t d_meta;
    // Metadata keys
    pmt::pmt_t d_samp_rate_key;
    pmt::pmt_t d_center_freq_key;
    pmt::pmt_t d_n_matrix_col_key;

    // time
    gr::high_res_timer_type d_update_time;
    gr::high_res_timer_type d_last_time;
    double                  d_update_sec {0.1}; // 초 단위 갱신 주기

    // averaging accumulator
    std::vector<double> d_accum_buf; // 누산(합계) 버퍼
    size_t              d_accum_cols {0}; // FFT 크기(열 수)
    size_t              d_accum_count {0}; // 주기 내 PDU 개수

    struct frame_row {
        long long ts_us;
        std::string device_id;
        double center_hz;
        double samp_rate_hz;
        double bin0_hz;
        double df_hz;
        int    fft_size;
        std::vector<float> power_db; // length = fft_size
    };
    // --- DB 옵션 (초기값; 환경변수/GRC로 교체 가능) ---
    bool        d_db_enable = false;
    std::string d_db_path   = "/var/tmp/spectrum.db";
    std::string d_device_id = "pluto-xx";

    // --- SQLite 비동기 writer 자원 ---
    sqlite3* d_db = nullptr;
    sqlite3_stmt* d_stmt = nullptr;

    std::thread              d_db_thread;
    std::mutex               d_m;
    std::condition_variable  d_cv;
    std::queue<frame_row>    d_q;
    bool                     d_stop = false;

    // --- DB 관련 메서드 선언 ---
    void db_open_and_prepare();
    void db_thread_loop();
    void db_close();
    void enqueue_frame(frame_row&& r);
    inline long long now_us() const;

public:
    spectro_sink_impl(double samp_rate,
                      int fft_size,
                      size_t ncol,
                      double center_freq,
                      QWidget* parent);
    ~spectro_sink_impl();

    bool start() override;
    bool stop() override;

    void exec_();
    QApplication* d_qapp;
    QWidget* qwidget();
#ifdef ENABLE_PYTHON
    PyObject* pyqwidget();
#else
    void* pyqwidget();
#endif
    void handle_rx_msg(pmt::pmt_t msg);
    void set_update_time(double t) override;
    void set_time_per_fft(double t);
    void set_msg_queue_depth(size_t) override;
    void set_metadata_keys(const std::string& samp_rate_key,
                           const std::string& n_matrix_col_key,
                           const std::string& center_freq_key) override;

    void set_db_enable(bool enable) override;
    void set_db_path(const std::string& path) override;
    void set_device_id(const std::string& id) override;
};

} // namespace plasma
} // namespace gr

#endif /* INCLUDED_PLASMA_SPECTRO_SINK_IMPL_H */