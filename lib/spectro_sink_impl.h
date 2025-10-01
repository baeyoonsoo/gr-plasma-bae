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

namespace gr {
namespace plasma {

class spectro_sink_impl : public spectro_sink
{
private:
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
    void set_metadata_keys(std::string samp_rate_key,
                           std::string n_matrix_col_key,
                           std::string center_freq_key) override;
};

} // namespace plasma
} // namespace gr

#endif /* INCLUDED_PLASMA_SPECTRO_SINK_IMPL_H */