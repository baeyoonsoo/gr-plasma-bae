#ifndef QT_SPEC_UPDATE_EVENTS
#define QT_SPEC_UPDATE_EVENTS

#include <QEvent>
#include <complex>
#include <pmt/pmt.h>
#include <vector>

static constexpr int RadarUpdateEventType = 4096;

class SpectroUpdateEvent : public QEvent
{
public:
    SpectroUpdateEvent(const double* data,
                            size_t rows,
                            size_t cols,
                            pmt::pmt_t meta);
    ~SpectroUpdateEvent() override;
    double* data();
    const size_t cols();
    const size_t rows();
    const pmt::pmt_t meta();
    static QEvent::Type Type() { return QEvent::Type(RadarUpdateEventType); }

private:
    double* d_data;
    size_t d_rows;
    size_t d_cols;
    pmt::pmt_t d_meta;
};

#endif /* QT_SPEC_UPDATE_EVENTS */
