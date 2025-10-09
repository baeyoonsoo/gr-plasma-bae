#include "spectro_window.h"
#include <gnuradio/plasma/qt_spec_update_events.h>
#include <iostream>
#include <qwt_plot_grid.h>

class ColorMap : public QwtLinearColorMap
{
public:
    ColorMap() : QwtLinearColorMap(Qt::darkBlue, Qt::darkRed)
    {
        addColorStop(0.2, Qt::blue);
        addColorStop(0.4, Qt::cyan);
        addColorStop(0.6, Qt::yellow);
        addColorStop(0.8, Qt::red);
    }
};

class MyZoomer : public QwtPlotZoomer
{
public:
    MyZoomer(QWidget* canvas) : QwtPlotZoomer(canvas) { setTrackerMode(AlwaysOn); }

    virtual QwtText trackerTextF(const QPointF& pos) const
    {
        QColor bg(Qt::white);
        bg.setAlpha(200);

        QwtText text = QwtPlotZoomer::trackerTextF(pos);
        text.setBackgroundBrush(QBrush(bg));
        return text;
    }
};

SpectroWindow::SpectroWindow(QWidget* parent,
                                       double samp_rate,
                                       double center_freq)
    : QWidget(parent), d_samp_rate(samp_rate), d_center_freq(center_freq)
{
    d_plot = new QwtPlot();
    d_plot->setCanvasBackground(Qt::white);

    // for spectro
    d_spectro = new QwtPlotSpectrogram();
    d_data = new SpectroData();
    d_spectro->setData(d_data);
    d_spectro->setColorMap(new ColorMap());
    d_spectro->attach(d_plot);

    QwtPlotGrid* grid = new QwtPlotGrid();
    grid->setMajorPen(QPen(Qt::gray, 0, Qt::DotLine));
    grid->attach(d_plot);

    // Plot zoomer setup
    d_zoomer = new MyZoomer(d_plot->canvas());
    d_zoomer->setMousePattern(
        QwtEventPattern::MouseSelect2, Qt::RightButton, Qt::ControlModifier);
    d_zoomer->setMousePattern(QwtEventPattern::MouseSelect3, Qt::RightButton);
    const QColor c(Qt::blue);
    d_zoomer->setRubberBandPen(c);
    d_zoomer->setTrackerPen(c);
    // Plot panner setup
    d_panner = new QwtPlotPanner(d_plot->canvas());
    d_panner->setAxisEnabled(QwtPlot::yRight, false);
    d_panner->setMouseButton(Qt::MiddleButton);

    // GUI layout
    v_layout = new QVBoxLayout();
    v_layout->addWidget(d_plot);
    setLayout(v_layout);

    d_closed = false;
    d_busy = false;
}

SpectroWindow::~SpectroWindow() { d_closed = true; }

bool SpectroWindow::is_closed() const { return d_closed; }

bool SpectroWindow::busy() const { return d_busy; }

void SpectroWindow::xlim(double x1, double x2)
{
    d_data->setInterval(Qt::XAxis, QwtInterval(x1, x2));
}

void SpectroWindow::ylim(double y1, double y2)
{
    d_data->setInterval(Qt::YAxis, QwtInterval(y1, y2));
}

void SpectroWindow::set_metadata_keys(std::string samp_rate_key,
                                    std::string n_matrix_col_key,
                                    std::string center_freq_key)
{
    d_samp_rate_key     = pmt::intern(samp_rate_key);
    d_n_matrix_col_key  = pmt::intern(n_matrix_col_key);
    d_center_freq_key   = pmt::intern(center_freq_key);
}

void SpectroWindow::customEvent(QEvent* e)
{
    d_busy = true;
    
    if (e->type() == SpectroUpdateEvent::Type()) {

        SpectroUpdateEvent* event = static_cast<SpectroUpdateEvent*>(e);
        pmt::pmt_t meta = event->meta();
        const double* data = event->data();

        const size_t N = static_cast<size_t>(pmt::to_long(pmt::dict_ref(meta, pmt::intern("fft_size"), pmt::from_long(1024))));
        d_samp_rate  = pmt::to_double(pmt::dict_ref(meta, pmt::intern("samp_rate"), pmt::from_double(d_samp_rate)));
        d_center_freq= pmt::to_double(pmt::dict_ref(meta, pmt::intern("center_freq"), pmt::from_double(d_center_freq)));
        d_cols = static_cast<int>(N);

        set_freq_axis(d_center_freq, d_samp_rate);

        d_water_values.reserve(d_water_values.size() + static_cast<int>(N));
        for (size_t i = 0; i < N; ++i) d_water_values.push_back(data[i]);

        int rows = d_water_values.size() / d_cols;
        if (rows > d_max_rows) {
            const int drop_rows = rows - d_max_rows;
            const int drop_elems = drop_rows * d_cols;
            if (drop_elems > 0 && drop_elems <= d_water_values.size()) { 
                d_water_values.erase(d_water_values.begin(),
                                    d_water_values.begin() + drop_elems);
            }
            rows = d_max_rows;
        }
        const uint64_t now_us = event->timestamp_us();
        const double   fp_s   = event->frame_period_s();
        const double   step_s = (fp_s > 0.0) ? fp_s : d_time_per_fft;
        const double now_sec   = (now_us > 0) ? (now_us / 1e6) : 0.0;
        if (!have_t0_) { t0_sec_ = now_sec; have_t0_ = true; }
        const double now_rel_s = now_sec - t0_sec_;   
        if (d_last_row_end_s == 0.0)
        d_last_row_end_s = now_rel_s;           
        d_last_row_end_s += step_s;

        d_data->setValueMatrix(d_water_values, d_cols);
        set_time_axis();
        d_plot->replot();
    }
    d_busy = false;
}

void SpectroWindow::set_time_axis()
{
    const double tmax = d_last_row_end_s;
    const double tmin = tmax - d_time_window_s;

    d_plot->setAxisTitle(QwtPlot::yLeft, QString("Time (s)"));
    d_plot->setAxisScale(QwtPlot::yLeft, tmin, tmax);

    // d_data->setInterval(Qt::YAxis, QwtInterval(tmin, tmax));
}


void SpectroWindow::set_freq_axis(double center_freq, double samp_rate)
{
    const double fmin = center_freq - samp_rate / 2.0;
    const double fmax = center_freq + samp_rate / 2.0;

    d_plot->setAxisScale(QwtPlot::xBottom, fmin, fmax);
    d_plot->setAxisTitle(QwtPlot::xBottom, QString("Frequency (Hz)"));

    d_data->setInterval(Qt::XAxis, QwtInterval(fmin, fmax));
}

// Override displayform SetUpdateTime() to set FFT time
void SpectroWindow::setUpdateTime(double t)
{
    d_time_per_fft = t;
}