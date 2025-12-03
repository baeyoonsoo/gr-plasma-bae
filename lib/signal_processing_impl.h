/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_PLASMA_SIGNAL_PROCESSING_IMPL_H
#define INCLUDED_PLASMA_SIGNAL_PROCESSING_IMPL_H

#include <gnuradio/plasma/signal_processing.h>
#include <gnuradio/plasma/pmt_constants.h>
#include <arrayfire.h>
#include <vector>
#include <pmt/pmt.h>
#include <string>

namespace gr {
namespace plasma {

class signal_processing_impl : public signal_processing
{
private:

    size_t fft_size;
    double sample_rate;
    size_t n_samples;
    bool fft_on;
    double pdu_duration;

    size_t d_msg_queue_depth;
    bool output_magnitude;

    bool use_fft_plan_cache;
    bool use_inplace_fft;
    size_t fft_plan_cache_size;


    int d_win;
    int window_type;
    double overlap_frac;
    size_t overlap_samples;
    std::vector<gr_complex> overlap_buffer;
    bool process_partials;
    
    

    pmt::pmt_t d_meta;
    pmt::pmt_t d_data;
    pmt::pmt_t d_rx_port;
    pmt::pmt_t d_time_port;
    pmt::pmt_t d_freq_port;
    
    void handle_rx_msg(pmt::pmt_t msg);

public:
    signal_processing_impl(size_t fft_size, double sample_rate, bool fft_on, int output_magnitude, int window_type/*, float overlap_frac*/);
    ~signal_processing_impl();

    void set_msg_queue_depth(size_t depth) override;
    void set_backend(Device::Backend backend) override;
    void set_metadata_keys(const std::string& n_pulse_cpi_key) override;
};

} // namespace plasma
} // namespace gr

#endif /* INCLUDED_PLASMA_SIGNAL_PROCESSING_IMPL_H */