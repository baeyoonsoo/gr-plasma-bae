#ifndef INCLUDED_PLUTO_RADAR_BAE_IMPL_H
#define INCLUDED_PLUTO_RADAR_BAE_IMPL_H

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
#include <gnuradio/plasma/pluto_radar_bae.h>
#include <gnuradio/plasma/pmt_constants.h>

namespace gr {
namespace plasma {

class pluto_radar_bae_impl : public pluto_radar_bae
{
public:
    //typedef std::shared_ptr<pluto_radar_bae_impl> sptr;

    // Factory
    // static sptr make(const std::string &uri,
    //                  unsigned long long frequency,
    //                  unsigned long samplerate,
    //                  unsigned long bandwidth,
    //                  bool rx1_en, bool rx2_en,
    //                  unsigned long buffer_size,
    //                  bool quadrature, bool rfdc, bool bbdc,
    //                  const char *gain1, double gain1_value,
    //                  const char *gain2, double gain2_value,
    //                  const char *rf_port_select,
    //                  const char *filter = "",
    //                  bool auto_filter = true);
    pluto_radar_bae_impl(const std::string &uri,
                         unsigned long long frequency,
                         unsigned long samplerate,
                         unsigned long bandwidth,
                         bool rx1_en, bool rx2_en,
                         unsigned long buffer_size,
                         bool quadrature, bool rfdc, bool bbdc,
                         const char *gain1, double gain1_value,
                         const char *gain2, double gain2_value,
                         const char *rf_port_select,
                         const char *filter,
                         bool auto_filter);

    // Destructor
    ~pluto_radar_bae_impl() override;

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
    
    void set_params(struct iio_device *phy,
		    const std::vector<std::string> &params);

    // Message handler
    // void handle_message(const pmt::pmt_t &msg);
    // Main RX loop
    void run();

    // Calibration file loader if needed
    void read_calibration_file(const std::string &filename);

    // IIO / Pluto members
    size_t sample_bytes;
    std::string               uri;
    struct iio_context       *ctx{nullptr};
    struct iio_device        *dev{nullptr}, *phy{nullptr};
    struct iio_buffer        *buf{nullptr};
    bool                      destroy_ctx{false};
    size_t                    buffer_size{0x8000};
    //std::vector<std::string>  channels;
    std::vector<struct iio_channel*> channel_list;

    // Context cache
    struct ctxInfo { std::string uri; struct iio_context *ctx; int count; };
    static std::vector<ctxInfo> contexts;
    typedef std::vector<ctxInfo>::iterator ctx_it;

    // Runtime configuration parameters
    unsigned long long        frequency{0};
    unsigned long             samplerate{0};
    unsigned long             bandwidth{0};
    bool                      rx1_en{true};     // I channel
    bool                      rx2_en{true};    // Q channel
    bool                      quadrature{true};
    bool                      rfdc{true};
    bool                      bbdc{true};
    std::string               gain1{"manual"};
    double                    gain1_value{0.0};
    std::string               gain2{"manual"};
    double                    gain2_value{0.0};
    std::string               rf_port_select{"A_BALANCED"};
    std::string               filter{""};
    bool                      auto_filter{true};

    // Conversion blocks
    gr::blocks::short_to_float::sptr s2f_i, s2f_q;
    gr::blocks::float_to_complex::sptr  f2c;
    std::vector<float>                temp_i, temp_q;

    // Threading
    std::thread                       refill_thread;
    std::thread                       rx_thread;
    std::mutex                        mtx;
    std::condition_variable           cv;
    bool                              data_ready{false};
    bool                              finished{false};

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

#endif /* INCLUDED_PLUTO_RADAR_BAE_IMPL_H */
