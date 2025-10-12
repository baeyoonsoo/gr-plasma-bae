/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_PLASMA_SPECTRO_SINK_H
#define INCLUDED_PLASMA_SPECTRO_SINK_H

#include <gnuradio/block.h>
#include <gnuradio/plasma/api.h>
#include <string>
#include <QWidget>

#ifdef ENABLE_PYTHON
struct _object;
using PyObject = _object;
#endif

namespace gr {
namespace plasma {

/*!
 * \brief Spectrogram sink (PDU -> time-frequency waterfall)
 * \ingroup plasma
 */
class PLASMA_API spectro_sink : virtual public gr::block
{
public:
    using sptr = std::shared_ptr<spectro_sink>;

    static sptr make(double   samp_rate,
                     int      fft_size,
                     size_t   ncol,
                     double   center_freq,
                     QWidget* parent = nullptr);

    virtual ~spectro_sink() = default;

    virtual void exec_() = 0;

    virtual QWidget* qwidget() = 0;

#ifdef ENABLE_PYTHON
    virtual PyObject* pyqwidget() = 0;
#else
    virtual void* pyqwidget() = 0;
#endif

    virtual void set_update_time(double seconds) = 0;
    virtual void set_msg_queue_depth(size_t depth) = 0;

    virtual void set_metadata_keys(const std::string& samp_rate_key,
                                   const std::string& n_matrix_col_key,
                                   const std::string& center_freq_key) = 0;
};

} // namespace plasma
} // namespace gr

#endif /* INCLUDED_PLASMA_SPECTRO_SINK_H */
