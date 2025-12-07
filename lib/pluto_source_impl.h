#ifndef INCLUDED_PLUTO_SOURCE_IMPL_H
#define INCLUDED_PLUTO_SOURCE_IMPL_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <iio.h>
#include <pmt/pmt.h>
#include <gnuradio/block.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/blocks/short_to_float.h>
#include <gnuradio/blocks/float_to_complex.h>
#include <gnuradio/plasma/pluto_source.h>
#include <gnuradio/plasma/pmt_constants.h>
#include <arrayfire.h>

namespace gr {
namespace plasma {

class pluto_source_impl : public pluto_source
{
public:

    // Factory

    pluto_source_impl(const std::string &uri,
                         double frequency,
                         double samplerate,
                         double bandwidth,
                         bool rx1_en, bool rx2_en,
                         unsigned long buffer_size,
                         bool quadrature, bool rfdc, bool bbdc,
                         const char *gain1, double gain1_value,
                         const char *gain2, double gain2_value,
                         const char *rf_port_select,
                         double d_noise_floor_dbm,
                         const char *filter,
                         bool auto_filter,
                         double pdu_duration);

    // Destructor
    ~pluto_source_impl() override;

    // Allow runtime metadata key setting
    void set_metadata_keys(const std::string &txk,
                           const std::string &rxk,
                           const std::string &sk);

    // Expose internal IIO context caching (shared contexts)
    static std::vector<std::string> get_channels_vector(bool ch1_en,
                                                        bool ch2_en/*,
                                                        bool ch3_en,
                                                        bool ch4_en*/);

    // GNU Radio block overrides
    bool start() override;
    bool stop() override;
    static bool load_fir_filter(std::string &filter, struct iio_device *phy);
private:
    
    // mutex
    std::mutex               refill_mtx;
    std::condition_variable  refill_cv;
    bool                     please_refill{false};
    bool                     thread_stopped{false};
    std::atomic<bool>        finished{false};
    
    void set_params(struct iio_device *phy,
		    const std::vector<std::string> &params);

    // Main RX loop
    void run();
    void receive();

    // Calibration file loader if needed
    void read_calibration_file(const std::string &filename);
    af::array af_fftshift1d(const af::array &x);

    // IIO / Pluto members
    size_t sample_bytes;
    std::string               uri;
    struct iio_context       *ctx{nullptr};
    struct iio_device        *dev{nullptr}, *phy{nullptr};
    struct iio_buffer        *buf{nullptr};
    bool                      destroy_ctx{false};
    size_t                    samples_per_pdu;
    std::vector<struct iio_channel*> channel_list;

    // Context cache
    struct ctxInfo { std::string uri; struct iio_context *ctx; int count; };
    static std::vector<ctxInfo> contexts;
    typedef std::vector<ctxInfo>::iterator ctx_it;

    // Runtime configuration parameters
    double                    frequency{0};
    double                    samplerate{0};
    double                    bandwidth{0};
    bool                      rx1_en{true};     // I channel
    bool                      rx2_en{true};     // Q channel
    size_t                    buffer_size{0x8000};
    bool                      quadrature{true};
    bool                      rfdc{true};
    bool                      bbdc{true};
    std::string               gain1{"manual"};
    double                    gain1_value{0.0};
    std::string               gain2{"manual"};
    double                    gain2_value{0.0};
    std::string               rf_port_select{"A_BALANCED"};
    double                    d_noise_floor_dbm{-30.0};
    std::string               filter{""};
    bool                      auto_filter{true};
    double                    pdu_duration{0.0};
    
    bool d_noise_floor_initialized{false};
    double d_noise_floor{0.0};
    double d_noise_alpha{0.01};
    double d_detect_threshold_db{6.0};

    std::chrono::steady_clock::time_point d_start_time = std::chrono::steady_clock::now();
    // Conversion blocks
    gr::blocks::short_to_float::sptr s2f_i, s2f_q;
    gr::blocks::float_to_complex::sptr  f2c;
    std::vector<float>                temp_i, temp_q;

    // Threading
    std::thread                       rx_thread;
    std::thread                       refill_thread;
    std::mutex                        mtx;
    std::condition_variable           cv;
    bool                              data_ready{false};

    // PDU metadata / TX state
    pmt::pmt_t                        next_meta;
    pmt::pmt_t                        tx_data;
    size_t                            tx_buff_size{0};
    
    // metadata keys
    std::string   tx_freq_key;
    std::string   rx_freq_key;
    std::string   sample_start_key;
};

} // namespace plasma
} // namespace gr

#endif /* INCLUDED_PLUTO_SOURCE_IMPL_H */