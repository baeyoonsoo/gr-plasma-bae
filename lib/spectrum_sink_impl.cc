/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "spectrum_sink_impl.h"
#include <gnuradio/io_signature.h>
#include <chrono>
#include <thread>

namespace gr {
namespace plasma {

spectrum_sink::sptr spectrum_sink::make(double samp_rate,
                              size_t ncol,
                              double center_freq,
                              QWidget* parent,
                              int mode)
{
    return gnuradio::make_block_sptr<spectrum_sink_impl>(
        samp_rate, ncol, center_freq, parent, mode);
}

/*
 * The private constructor
 */
spectrum_sink_impl::spectrum_sink_impl(double samp_rate,
                             size_t ncol,
                             double center_freq,
                             QWidget* parent,
                             int mode)
    : gr::block("spectrum_sink",
                gr::io_signature::make(0, 0, 0),
                gr::io_signature::make(0, 0, 0)),
      d_samp_rate(samp_rate),
      d_ncol(ncol),
      d_center_freq(center_freq),
      mode(mode)
{
    parent = nullptr;
    // Initialize the QApplication
    d_argc = 1;
    d_argv = new char;
    d_argv[0] = '\0';
    if (qApp != NULL)
        d_qapp = qApp;
    else
        d_qapp = new QApplication(d_argc, &d_argv);
    d_main_gui = new RangeDopplerWindow(parent, samp_rate, center_freq, mode);

    // Initialize message ports
    d_in_port = PMT_IN;
    message_port_register_in(d_in_port);
    set_msg_handler(d_in_port, [this](pmt::pmt_t msg) { handle_rx_msg(msg); });
}

/*
 * Our virtual destructor.
 */
spectrum_sink_impl::~spectrum_sink_impl() { delete d_argv; }

bool spectrum_sink_impl::start()
{
    d_finished = false;
    return block::start();
}

bool spectrum_sink_impl::stop()
{
    d_finished = true;
    if (not d_main_gui->is_closed())
        d_main_gui->close();
    return block::stop();
}

void spectrum_sink_impl::exec_() { d_qapp->exec(); };

QWidget* spectrum_sink_impl::qwidget() { return (QWidget*)d_main_gui; }

#ifdef ENABLE_PYTHON
PyObject* spectrum_sink_impl::pyqwidget()
{
    PyObject* w = PyLong_FromVoidPtr((void*)d_main_gui);
    PyObject* retarg = Py_BuildValue("N", w);
    return retarg;
}
#else
void* spectrum_sink_impl::pyqwidget() { return nullptr; }
#endif

void spectrum_sink_impl::handle_rx_msg(pmt::pmt_t msg)
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
    }

    size_t n = pmt::length(samples);
    size_t nrow = n / d_ncol;

    if (pmt::is_f32vector(samples)) {
        const float* in = pmt::f32vector_elements(samples, n);
        double* out = new double[n];

        for (size_t i = 0; i < n; ++i) {
            out[i] = static_cast<double>(in[i]);
        }

        d_qapp->postEvent(d_main_gui, new RangeDopplerUpdateEvent(out, nrow, d_ncol, d_meta));
        delete[] out;

    } else if (pmt::is_c32vector(samples)) {
        const gr_complex* in = pmt::c32vector_elements(samples, n);
        double* out = new double[n];

        for (size_t i = 0; i < n; ++i) {
            
            out[i] = static_cast<double>(std::abs(in[i])); 
        }

        d_qapp->postEvent(d_main_gui, new RangeDopplerUpdateEvent(out, nrow, d_ncol, d_meta));
        delete[] out;
    }
}

void spectrum_sink_impl::set_dynamic_range(const double r)
{
    d_dynamic_range_db = r;
}

void spectrum_sink_impl::set_msg_queue_depth(size_t depth)
{
    d_msg_queue_depth = depth;
}

void spectrum_sink_impl::set_metadata_keys(std::string samp_rate_key,
                                                std::string n_matrix_col_key,
                                                std::string center_freq_key,
                                                std::string dynamic_range_key,
                                                std::string prf_key,
                                                std::string pulsewidth_key,
                                                std::string detection_indices_key)
{
    d_samp_rate_key     = pmt::intern(samp_rate_key);
    d_n_matrix_col_key  = pmt::intern(n_matrix_col_key);
    d_center_freq_key   = pmt::intern(center_freq_key);
    d_dynamic_range_key = pmt::intern(dynamic_range_key);
    // TODO: Pass the last 4 keys to the window object
    d_main_gui->set_metadata_keys(prf_key, pulsewidth_key, samp_rate_key, center_freq_key, detection_indices_key);
}

} /* namespace plasma */
} /* namespace gr */