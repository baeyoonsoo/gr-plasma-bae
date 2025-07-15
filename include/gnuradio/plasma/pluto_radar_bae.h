/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_PLASMA_PLUTO_RADAR_BAE_H
#define INCLUDED_PLASMA_PLUTO_RADAR_BAE_H

#include <gnuradio/block.h>
#include <gnuradio/plasma/api.h>

namespace gr {
namespace plasma {

/*!
 * \brief A block that simultaneously transmits and receives data from a USRP device
 * \ingroup plasma
 *
 */
class PLASMA_API pluto_radar_bae : virtual public gr::block
{
public:
    typedef std::shared_ptr<pluto_radar_bae> sptr;
    
    /*!
     * \brief Return a shared_ptr to a new instance of plasma::usrp_radar.
     *
     * To avoid accidental use of raw pointers, plasma::usrp_radar's
     * constructor is in a private implementation
     * class. plasma::usrp_radar::make is the public interface for
     * creating new instances.
     */
    static sptr make(const std::string &uri,
                 unsigned long long frequency,
                 unsigned long samplerate,
                 unsigned long bandwidth,
                 bool rx1_en, bool rx2_en,
                 unsigned long buffer_size,
                 bool quadrature, bool rfdc, bool bbdc,
                 const char *gain1, double gain1_value,
                 const char *gain2, double gain2_value,
                 const char *rf_port_select,
                 const char *filter = "",
                 bool auto_filter = true);
    virtual void set_metadata_keys(const std::string& tx_freq_key,
                                   const std::string& rx_freq_key,
                                   const std::string& sample_start_key) = 0;
};

} // namespace plasma
} // namespace gr

#endif /* INCLUDED_PLASMA_PLUTO_RADAR_BAE_H */
