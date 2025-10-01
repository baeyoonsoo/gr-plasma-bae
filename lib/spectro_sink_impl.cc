/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <Python.h>
#include "spectro_sink_impl.h"
#include <gnuradio/io_signature.h>
#include <QApplication>
#include <chrono>
#include <thread>


namespace gr {
namespace plasma {

spectro_sink::sptr spectro_sink::make(double samp_rate,
                              int fft_size,
                              size_t ncol,
                              double center_freq,
                              QWidget* parent)
{
    return gnuradio::make_block_sptr<spectro_sink_impl>(
        samp_rate, fft_size, ncol, center_freq, parent);
}

/*
 * The private constructor
 */
spectro_sink_impl::spectro_sink_impl(double samp_rate,
                             int fft_size,
                             size_t ncol,
                             double center_freq,
                             QWidget* parent)
    : gr::block("spectro_sink",
                gr::io_signature::make(0, 0, 0),
                gr::io_signature::make(0, 0, 0)),
      d_samp_rate(samp_rate),
      d_ncol(ncol),
      d_fft_size(fft_size),
      d_center_freq(center_freq)
{
    // Initialize the QApplication
    static int argc = 1;
    static char app[] = "spectro";
    static char* argv[] = { app, nullptr };

    if (qApp)
    d_qapp = qApp;
    else
    d_qapp = new QApplication(argc, argv);
        
    d_main_gui = new SpectroWindow(parent, samp_rate, center_freq);

    // time set
    set_update_time(0.1);

    // Initialize message ports
    d_in_port = PMT_IN;
    message_port_register_in(d_in_port);
    set_msg_handler(d_in_port, [this](pmt::pmt_t msg) { handle_rx_msg(msg); });
}

/*
 * Our virtual destructor.
 */
spectro_sink_impl::~spectro_sink_impl() { }

bool spectro_sink_impl::start()
{
    d_finished = false;
    return block::start();
}

bool spectro_sink_impl::stop()
{
    d_finished = true;
    if (not d_main_gui->is_closed())
        d_main_gui->close();
    return block::stop();
}

void spectro_sink_impl::exec_() { d_qapp->exec(); };

QWidget* spectro_sink_impl::qwidget() { return (QWidget*)d_main_gui; }

#ifdef ENABLE_PYTHON
PyObject* spectro_sink_impl::pyqwidget()
{
    PyObject* w = PyLong_FromVoidPtr((void*)d_main_gui);
    PyObject* retarg = Py_BuildValue("N", w);
    return retarg;
}
#else
void* spectro_sink_impl::pyqwidget() { return nullptr; }
#endif

void spectro_sink_impl::handle_rx_msg(pmt::pmt_t msg)
{
    if (d_main_gui->busy() or this->nmsgs(d_in_port) > d_msg_queue_depth) {
        return;
    }
    pmt::pmt_t samples;
    if (pmt::is_pdu(msg)) {
        samples = pmt::cdr(msg);
        d_meta = pmt::car(msg);
    } else if (pmt::is_uniform_vector(msg)) {
        samples = msg;
    } else {
        return;
    }

    size_t len = pmt::length(samples);
    if (len==0) return;

    size_t nrow = 1;
    size_t ncol = len;
    double* out = new double[len];

    if (pmt::is_f32vector(samples)) {
        const float* in = pmt::f32vector_elements(samples, len);
        for (size_t i = 0; i < len; ++i) {
            out[i] = static_cast<double>(in[i]);
        }
    } else if (pmt::is_c32vector(samples)){
        const gr_complex* in = pmt::c32vector_elements(samples, len);
        for (size_t i = 0; i < len; ++i) {
            out[i] = static_cast<double>(std::abs(in[i]));
        }
    } else {
        delete[] out;
        return;
    }

    // meta
    size_t N = 1024;
    d_fft_size = pmt::to_long(pmt::dict_ref(d_meta, pmt::intern("fft_size"), pmt::from_long(static_cast<long>(N))));
    d_samp_rate = pmt::to_double(pmt::dict_ref(d_meta, pmt::intern("samp_rate"), pmt::from_double(d_samp_rate)));
    d_center_freq = pmt::to_double(pmt::dict_ref(d_meta, pmt::intern("center_freq"), pmt::from_double(d_center_freq)));

    // time
    uint64_t now_us = 0;   
    double frame_period_s = 0.0;

    if (gr::high_res_timer_now() - d_last_time > d_update_time) {
        d_last_time = gr::high_res_timer_now();

        if (d_samp_rate <= 0.0) {
            set_time_per_fft(0.0);
        } else {
            if (d_samp_rate > 0.0) {
                int stride = std::max(0, static_cast<int>(len - static_cast<size_t>(d_fft_size)));
                frame_period_s = (stride > 0) ? (static_cast<double>(stride) / d_samp_rate)
                                            : (static_cast<double>(d_fft_size) / d_samp_rate);
            }
            set_time_per_fft(frame_period_s);

            const auto now_ticks = gr::high_res_timer_now();
            const auto tps       = gr::high_res_timer_tps();
            now_us = static_cast<uint64_t>((now_ticks * 1000000.0) / tps);
        }
                                        
    }

    d_qapp->postEvent(d_main_gui, new SpectroUpdateEvent(out, nrow, ncol, d_meta, now_us, frame_period_s));
    delete[] out;
}

void spectro_sink_impl::set_update_time(double t)
{
    // convert update time to ticks
    gr::high_res_timer_type tps = gr::high_res_timer_tps();
    d_update_time = t * tps;
    d_main_gui->setUpdateTime(t);
    d_last_time = 0;
}

void spectro_sink_impl::set_msg_queue_depth(size_t depth)
{
    d_msg_queue_depth = depth;
}

void spectro_sink_impl::set_metadata_keys(const std::string& samp_rate_key,
                                            const std::string& n_matrix_col_key,
                                            const std::string& center_freq_key)
{
    d_samp_rate_key     = pmt::intern(samp_rate_key);
    d_n_matrix_col_key  = pmt::intern(n_matrix_col_key);
    d_center_freq_key   = pmt::intern(center_freq_key);
    d_main_gui->set_metadata_keys(samp_rate_key, n_matrix_col_key, center_freq_key);
    // throw std::runtime_error("1\n");
}

void spectro_sink_impl::set_time_per_fft(double t) { 
    d_main_gui->setUpdateTime(t); 
}

} /* namespace plasma */
} /* namespace gr */