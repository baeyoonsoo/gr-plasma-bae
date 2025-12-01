// qt_spec_update_events.h
#ifndef QT_SPEC_UPDATE_EVENTS
#define QT_SPEC_UPDATE_EVENTS

#include <QEvent>
#include <pmt/pmt.h>
#include <vector>
#include <cstdint>
#include <cstddef>

class SpectroUpdateEvent : public QEvent
{
public:
    static QEvent::Type Type();

    SpectroUpdateEvent(const double* data,
                       size_t rows,
                       size_t cols,
                       pmt::pmt_t meta = pmt::PMT_NIL,
                       uint64_t timestamp_us = 0,
                       double frame_period_s = 0.0);

    const double* data()           const { return d_buf.data(); }
    size_t        rows()           const { return d_rows; }
    size_t        cols()           const { return d_cols; }
    pmt::pmt_t    meta()           const { return d_meta; }
    uint64_t      timestamp_us()   const { return d_ts_us; }
    double        frame_period_s() const { return d_fp_s; }

private:
    std::vector<double> d_buf;
    size_t              d_rows {0};
    size_t              d_cols {0};
    pmt::pmt_t          d_meta {pmt::PMT_NIL};
    uint64_t            d_ts_us {0};
    double              d_fp_s  {0.0};
};

#endif /* QT_SPEC_UPDATE_EVENTS */
