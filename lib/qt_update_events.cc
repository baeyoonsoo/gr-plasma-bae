#include <gnuradio/plasma/qt_update_events.h>
#include <pmt/pmt.h>
#include <iostream>

SpectrumUpdateEvent::SpectrumUpdateEvent(const double* data,
                                                 size_t rows,
                                                 size_t cols,
                                                 pmt::pmt_t meta)
    : QEvent(QEvent::Type(RadarUpdateEventType))
{
    d_rows = rows;
    d_cols = cols;
    d_data = new double[rows * cols];
    memcpy(d_data, data, rows * cols * sizeof(double));
    d_meta = meta;
}

SpectrumUpdateEvent::~SpectrumUpdateEvent() { delete[] d_data; }

const size_t SpectrumUpdateEvent::cols() { return d_cols; }

const size_t SpectrumUpdateEvent::rows() { return d_rows; }

double* SpectrumUpdateEvent::data() { return d_data; }

const pmt::pmt_t SpectrumUpdateEvent::meta() { return d_meta; }