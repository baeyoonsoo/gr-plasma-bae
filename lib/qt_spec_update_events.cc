#include <gnuradio/plasma/qt_spec_update_events.h>
#include <algorithm>

QEvent::Type SpectroUpdateEvent::Type()
{
    static const int type_id = QEvent::registerEventType();
    return static_cast<QEvent::Type>(type_id);
}

SpectroUpdateEvent::SpectroUpdateEvent(const double* src,
                                       size_t rows,
                                       size_t cols,
                                       pmt::pmt_t meta,
                                       uint64_t timestamp_us,
                                       double   frame_period_s)
    : QEvent(SpectroUpdateEvent::Type())
    , d_rows(rows)
    , d_cols(cols)
    , d_meta(meta)
    , d_ts_us(timestamp_us)
    , d_fp_s(frame_period_s)
{
    const size_t n = rows * cols;
    d_buf.resize(n);
    if (src && n > 0) {
        std::copy(src, src + n, d_buf.begin());
    }
}
