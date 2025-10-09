#ifndef INCLUDED_PLASMA_SPECTRO_WINDOW_H
#define INCLUDED_PLASMA_SPECTRO_WINDOW_H
#include <gnuradio/plasma/pmt_constants.h>
#include <gnuradio/plasma/qt_spec_update_events.h>
// #include <plasma_dsp/file.h>
// #include <plasma_dsp/lfm.h>

#include <pmt/pmt.h>

#include <qwt/qwt_plot.h>
#include <qwt/qwt_thermo.h>
#include <qwt_color_map.h>
#include <qwt_matrix_raster_data.h>
#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_renderer.h>
#include <qwt_plot_spectrogram.h>
#include <qwt_plot_zoomer.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>

#include <QBoxLayout>
#include <QCheckBox>
#include <QWidget>

#include <pmt/pmt.h>
#include <complex>
#include <iostream>
#include <vector>
#include <atomic>

class SpectroData : public QwtMatrixRasterData
{
public:
    SpectroData() : QwtMatrixRasterData() {}

    virtual void setValueMatrix(const QVector<double>& values, int numColumns)
    {
        this->values = values;
        this->numColumns = numColumns;
        numRows = values.size() / numColumns;

        const QwtInterval xInterval = interval(Qt::XAxis);
        const QwtInterval yInterval = interval(Qt::YAxis);
        if (xInterval.isValid())
            dx = xInterval.width() / numColumns;
        if (yInterval.isValid())
            dy = yInterval.width() / numRows;
    }

    virtual void setInterval(Qt::Axis axis, const QwtInterval& interval)
    {
        QwtRasterData::setInterval(axis, interval);
        if (axis == Qt::XAxis) {
            dx = interval.width() / numColumns;
        } else if (axis == Qt::YAxis) {
            dy = interval.width() / numRows;
        }
    }

    virtual double value(double x, double y) const
    {
        const QwtInterval xInterval = interval(Qt::XAxis);
        const QwtInterval yInterval = interval(Qt::YAxis);

        if (!(xInterval.contains(x) && yInterval.contains(y)))
            return qQNaN();

        double value;

        int row = round((y - yInterval.minValue()) / dy);
        int col = round((x - xInterval.minValue()) / dx);

        if (row < 0) row = 0;
        if (col < 0) col = 0;
        if (row >= numRows)    row = numRows - 1;
        if (col >= numColumns) col = numColumns - 1;

        value = values[row * numColumns + col];

        return value;
    }

private:
    QVector<double> values;
    int numColumns;
    int numRows;
    double dx;
    double dy;
};

class SpectroWindow : public QWidget
{
    Q_OBJECT

public:
    SpectroWindow(QWidget* parent = nullptr,
                       double samp_rate = 0,
                       double center_freq = 0);
    ~SpectroWindow();

    bool is_closed() const;
    bool busy() const;

    void xlim(double x1, double x2);
    void ylim(double y1, double y2);


    void set_metadata_keys(std::string samp_rate_key,
                           std::string n_matrix_col_key,
                           std::string center_freq_key);

    void setUpdateTime(double t);
    void customEvent(QEvent* e) override;

private:
    void set_time_axis(); 
    void set_freq_axis(double center_freq, double samp_rate);

    // Qwt plot objects
    QwtPlotSpectrogram* d_spectro;
    QwtPlot* d_plot;
    SpectroData* d_data;
    QwtPlotZoomer* d_zoomer;
    QwtPlotPanner* d_panner;

    // QT widgets
    QVBoxLayout* v_layout;

    // Parameters
    double d_samp_rate = 0;
    double d_center_freq = 0;

    // Status variables
    std::atomic<bool> d_busy;
    bool d_closed;

    // time
    bool   have_t0_ = false;
    double t0_sec_  = 0.0;
    
    // Waterfall state
    QVector<double> d_water_values; // (rows × d_cols) flattened row-major
    int    d_cols            = 0;   // FFT size (num columns)
    int    d_max_rows        = 512; // 화면에 유지할 최대 행 수
    double d_time_per_fft    = 0.0; // 프레임 간격(초) – sink에서 세팅 or 이벤트 fp_s
    double d_time_window_s   = 5.0; // 최근 몇 초를 표시할지
    double d_last_row_end_s  = 0.0; // y축 최대(현재 끝 시각)

    pmt::pmt_t d_samp_rate_key;
    pmt::pmt_t d_n_matrix_col_key;
    pmt::pmt_t d_center_freq_key;
};

#endif /* INCLUDED_PLASMA_SPECTRO_WINDOW_H */