/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_PLASMA_SIGNAL_PROCESSING_H
#define INCLUDED_PLASMA_SIGNAL_PROCESSING_H

#include <gnuradio/block.h>
#include <gnuradio/plasma/api.h>
#include <gnuradio/plasma/device.h>
#include <string>
#include <cstddef>

namespace gr {
namespace plasma {

/*!
 * \brief 
 * \ingroup plasma
 */
class PLASMA_API signal_processing : virtual public gr::block
{
public:
    typedef std::shared_ptr<signal_processing> sptr;

    enum WindowType {
        WINDOW_NONE = 0,
        WINDOW_HANN = 1,
        WINDOW_HAMMING = 2
    };

    /*!
     * \brief 
     * \param fft_size
     * \param sample_rate
     * \param fft_on
     * \param output_magnitude
     * \param window_type
     * \param overlap_frac
     */
    static sptr make(size_t fft_size, double sample_rate, bool fft_on, int output_magnitude, int window_type, float overlap_frac);

    virtual void set_msg_queue_depth(size_t depth) = 0;
    virtual void set_backend(Device::Backend backend) = 0;
    virtual void set_metadata_keys(const std::string& n_pulse_cpi_key) = 0;

};

} // namespace plasma
} // namespace gr

#endif /* INCLUDED_PLASMA_SIGNAL_PROCESSING_H */
