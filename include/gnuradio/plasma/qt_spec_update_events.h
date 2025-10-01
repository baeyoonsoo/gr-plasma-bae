#ifndef QT_SPEC_UPDATE_EVENTS
#define QT_SPEC_UPDATE_EVENTS

#include <QEvent>
#include <complex>
#include <pmt/pmt.h>
#include <vector>

// static constexpr int RadarUpdateEventType = 4096;

class SpectroUpdateEvent : public QEvent
{
public:
    SpectroUpdateEvent(const double* data,
                            size_t rows,
                            size_t cols,
                            uint64_t timestamp_us = 0,
                            double   frame_period_s = 0.0)
        : QEvent(static_cast<QEvent::Type>(SpectroUpdateEventType))
        , d_data(data)
        , d_rows(rows)
        , d_cols(cols)
        , d_meta(meta)
        , d_ts_us(timestamp_us)
        , d_fp_s(frame_period_s)
    {}

    ~SpectroUpdateEvent() override = default;

    const double* data()        const { return d_data; }
    size_t        rows()        const { return d_rows; }
    size_t        cols()        const { return d_cols; }
    pmt::pmt_t    meta()        const { return d_meta; }
    uint64_t      timestamp_us()const { return d_ts_us; }
    double        frame_period_s() const { return d_fp_s; }

    static QEvent::Type Type() { return static_cast<QEvent::Type>(SpectroUpdateEventType); }


private:
    const double* d_data {nullptr};
    size_t        d_rows {0};
    size_t        d_cols {0};
    pmt::pmt_t    d_meta {};
    uint64_t      d_ts_us {0};
    double        d_fp_s  {0.0};
};

#endif /* QT_SPEC_UPDATE_EVENTS */
