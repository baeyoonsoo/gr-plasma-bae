#include "range_doppler_window.h"
#include <iostream>

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

RangeDopplerWindow::RangeDopplerWindow(QWidget* parent,
                                       double samp_rate,
                                       double center_freq)
    : QWidget(parent), d_samp_rate(samp_rate), d_center_freq(center_freq)
{
    
    d_curve = new QwtPlotCurve();

    // Spectrogram
    d_plot = new QwtPlot();
    d_spectro = new QwtPlotSpectrogram();
    d_spectro->setColorMap(new ColorMap());
    d_spectro->attach(d_plot);
    d_data = new RangeDopplerData();
    d_plot->setAutoReplot(true);

    // Colorbar setup
    QwtScaleWidget* rightAxis = d_plot->axisWidget(QwtPlot::yRight);
    rightAxis->setTitle("QWER");
    rightAxis->setColorBarEnabled(true);
    d_plot->enableAxis(QwtPlot::yRight);

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
    d_prf = 0;
    d_pulsewidth = 0;
    d_mode = 1;
}

RangeDopplerWindow::~RangeDopplerWindow() { d_closed = true; }

bool RangeDopplerWindow::is_closed() const { return d_closed; }

bool RangeDopplerWindow::busy() const { return d_busy; }

void RangeDopplerWindow::xlim(double x1, double x2)
{
    d_data->setInterval(Qt::XAxis, QwtInterval(x1, x2));
}

void RangeDopplerWindow::ylim(double y1, double y2)
{
    d_data->setInterval(Qt::YAxis, QwtInterval(y1, y2));
}


void RangeDopplerWindow::show_detections(bool checked)
{
    if (checked)
        d_curve->attach(d_plot);

    else
        d_curve->detach();
}

void RangeDopplerWindow::set_metadata_keys(std::string prf_key,
                                           std::string pulsewidth_key,
                                           std::string samp_rate_key,
                                           std::string center_freq_key,
                                           std::string detection_indices_key)
{
    d_prf_key = pmt::intern(prf_key);
    d_pulsewidth_key = pmt::intern(pulsewidth_key);
    d_samp_rate_key = pmt::intern(samp_rate_key);
    d_center_freq_key = pmt::intern(center_freq_key);
    d_detection_indices_key = pmt::intern(detection_indices_key);
}

void RangeDopplerWindow::customEvent(QEvent* e)
{
    d_busy = true;
    
    if (e->type() == RangeDopplerUpdateEvent::Type()) {

        RangeDopplerUpdateEvent* event = static_cast<RangeDopplerUpdateEvent*>(e);
        double* data = event->data();
        const size_t rows = event->rows();
        const size_t cols = event->cols();
        const size_t N = 1024; // need to change into fft_size meta data

        if (N == 0) {
            d_busy = false;
            return;
        }

        pmt::pmt_t meta = event->meta();
        d_samp_rate = pmt::to_double(pmt::dict_ref(meta, d_samp_rate_key, pmt::from_double(d_samp_rate)));
        d_center_freq = pmt::to_double(pmt::dict_ref(meta, d_center_freq_key, pmt::from_double(d_center_freq)));

        QVector<double> x(N);
        const double fs = d_samp_rate > 0 ? d_samp_rate : 1.0;
        const double df = fs / static_cast<double>(N);

        const double start = -fs / 2.0;
        for (size_t i = 0; i < N; ++i) {
            x[static_cast<int>(i)] = start + i * df;
        }

        QVector<double> y(N);
        std::copy(data, data + N, y.data());

        set_range_axis();
        set_velocity_axis();

        if(d_mode == 0) {
            if (d_curve->plot() == nullptr) {
            d_curve->attach(d_plot);
        }
            d_curve->setSamples(x.constData(),
                                y.constData(),
                                static_cast<int>(N));
        } else if (d_mode == 1) {
            if (d_max_hold.size() != N)
            {
                d_max_hold.resize(N);
                for (int i = 0; i < N; ++i)
                    d_max_hold[i] = y[i];
                
                if (!d_max_hold_curve) {
                    d_max_hold_curve = new QwtPlotCurve();
                    d_max_hold_curve->attach(d_plot);
                }
            }
            else
            {
               for (int i = 0; i < N; ++i)
                    if (y[i] > d_max_hold[i])
                        d_max_hold[i] = y[i];
            }
            d_max_hold_curve->setSamples(x.constData(), d_max_hold.constData(), static_cast<int>(N));
        }
        else if (d_mode == 2)
        {
            if (d_avg.size() != N)
            {
                d_avg.resize(N);
                for (int i = 0; i < N; ++i)
                    d_avg[i] = y[i];

                if (!d_avg_curve)
                {
                    d_avg_curve = new QwtPlotCurve();
                    d_avg_curve->attach(d_plot);
                }
            } else
            {
                for (int i = 0; i < N; ++i)
                    d_avg[i] = d_avg_alpha * y[i] + (1.0 - d_avg_alpha) * d_avg[i];
            }
            d_avg_curve->setSamples(x.constData(), d_avg.constData(), static_cast<int>(N));
        }
        d_plot->replot();
    }
    d_busy = false;
}


void RangeDopplerWindow::set_range_axis()
{
    const double c = ::plasma::physconst::c;
    double rmin = -140;
    double rmax = 10;
    ylim(rmin, rmax);
    // Update the axes if we're plotting range and doppler
    QwtScaleWidget* y = d_plot->axisWidget(QwtPlot::yLeft);
    y->setTitle("ABDC");
    y = d_plot->axisWidget(QwtPlot::xBottom);
}

void RangeDopplerWindow::set_velocity_axis()
{
    const double c = ::plasma::physconst::c;
    // Center frequency not specified...Can set up the doppler axis but not velocity
    double dmin = -512;
    double dmax = 512;
    xlim(dmin, dmax);
    // Update the axes if we're plotting range and doppler
    QwtScaleWidget* y = d_plot->axisWidget(QwtPlot::yLeft);
    y = d_plot->axisWidget(QwtPlot::xBottom);
    y->setTitle("EFGH");
}

void RangeDopplerWindow::plot_detections(pmt::pmt_t indices, int nrow, int ncol)
{

    if (not pmt::is_null(indices)) {
        std::vector<int> idx = pmt::s32vector_elements(indices);

        QVector<double> xData(idx.size());
        QVector<double> yData(idx.size());
        for (size_t i = 0; i < idx.size(); i++) {
            int detection_col = idx[i] / nrow;
            int detection_row = idx[i] % nrow;
            // Compute the x and y values
            xData[i] = detection_col / (float)ncol * d_data->interval(Qt::XAxis).width() +
                       d_data->interval(Qt::XAxis).minValue();
            yData[i] = detection_row / (float)nrow * d_data->interval(Qt::YAxis).width() +
                       d_data->interval(Qt::YAxis).minValue();
        }

        d_curve->setSamples(xData, yData);
    }
}