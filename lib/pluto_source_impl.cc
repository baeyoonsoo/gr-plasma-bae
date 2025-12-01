// pluto_source_impl.cc
#include "pluto_source_impl.h"
#include <gnuradio/plasma/pluto_source.h>
#include <gnuradio/io_signature.h>

#include <gnuradio/blocks/short_to_float.h>
#include <gnuradio/blocks/float_to_complex.h>
#include <pmt/pmt.h>
#include <chrono>
#include <stdexcept>
#include <iio.h>


namespace gr {
namespace plasma {

pluto_source::sptr pluto_source::make(const std::string &uri,
                 double frequency,
                 unsigned long samplerate,
                 unsigned long bandwidth,
                 bool rx1_en, bool rx2_en,
                 unsigned long buffer_size,
                 bool quadrature, bool rfdc, bool bbdc,
                 const char *gain1, double gain1_value,
                 const char *gain2, double gain2_value,
                 const char *rf_port_select,
                 const char *filter,
                 bool auto_filter,
                 double pdu_duration)
{
    return gnuradio::make_block_sptr<pluto_source_impl>(uri,
        frequency,
        samplerate,
        bandwidth,
        rx1_en,
        rx2_en,
        buffer_size,
        quadrature,
        rfdc,
        bbdc,
        gain1,
        gain1_value,
        gain2,
        gain2_value,
        rf_port_select,
        filter,
        auto_filter,
        pdu_duration
    );
}

void pluto_source_impl::set_params(struct iio_device *phy,
		    const std::vector<std::string> &params)
{
    for (std::vector<std::string>::const_iterator it = params.begin();
            it != params.end(); ++it) {
        struct iio_channel *chn = NULL;
        const char *attr = NULL;
        size_t pos;
        int ret;

        pos = it->find('=');
        if (pos == std::string::npos) {
            std::cerr << "Misformed line: " << *it << std::endl;
            continue;
        }

        std::string key = it->substr(0, pos);
        std::string val = it->substr(pos + 1, std::string::npos);

        ret = iio_device_identify_filename(phy,
                key.c_str(), &chn, &attr);
        if (ret) {
            std::cerr << "Parameter not recognized: "
                << key << std::endl;
            continue;
        }

        if (chn)
            ret = iio_channel_attr_write(chn,
                    attr, val.c_str());
        else if (iio_device_find_attr(phy, attr))
            ret = iio_device_attr_write(phy, attr, val.c_str());
        else
            ret = iio_device_debug_attr_write(phy,
                    attr, val.c_str());
        if (ret < 0) {
            std::cerr << "Unable to write attribute " << key
                <<  ": " << ret << std::endl;
        }
    }
}
pluto_source_impl::pluto_source_impl(const std::string &uri,
                 double frequency,
                 unsigned long samplerate,
                 unsigned long bandwidth,
                 bool rx1_en, bool rx2_en,
                 unsigned long buffer_size,
                 bool quadrature, bool rfdc, bool bbdc,
                 const char *gain1, double gain1_value,
                 const char *gain2, double gain2_value,
                 const char *rf_port_select,
                 const char *filter = "",
                 bool auto_filter = true,
                 double pdu_duration = 100e-6)
    : gr::block("pluto_source", gr::io_signature::make(0, 0, 0), gr::io_signature::make(0, 0, 0)),
      uri(uri),
      frequency(frequency), 
      samplerate(samplerate),
      bandwidth(bandwidth), 
      rx1_en(rx1_en),
      rx2_en(rx2_en), 
      buffer_size(buffer_size),
      quadrature(quadrature), 
      rfdc(rfdc),
      bbdc(bbdc), 
      gain1(gain1),
      gain1_value(gain1_value),
      gain2(gain2),
      gain2_value(gain2_value),
      rf_port_select(rf_port_select),
      filter(filter),
      auto_filter(auto_filter),
      pdu_duration(pdu_duration)
{
    message_port_register_in(pmt::mp("in"));
    message_port_register_out(pmt::mp("out"));
    // set_msg_handler(PMT_IN, [this](const pmt::pmt_t& msg) { handle_message(msg); });
}

pluto_source_impl::~pluto_source_impl() {}

std::vector<std::string> pluto_source_impl::get_channels_vector(
        bool ch1_en, bool ch2_en/*, bool ch3_en, bool ch4_en*/)
{
    std::vector<std::string> channels;
    if (ch1_en)
        channels.push_back("voltage0");
    if (ch2_en)
        channels.push_back("voltage1");
    // if (ch3_en)
    //     channels.push_back("voltage2");
    // if (ch4_en)
    //     channels.push_back("voltage3");
    return channels;
}

bool pluto_source_impl::start()
{
    unsigned int nb_channels, i;
	unsigned short vid, pid;
    // IIO setup
    // 1. context, device, physical
    ctx = uri.empty() ? iio_create_default_context() : iio_create_context_from_uri(uri.c_str());
    if (!ctx)
        throw std::runtime_error("Unable to create context");
    destroy_ctx = true;
    dev = iio_context_find_device(ctx, "cf-ad9361-lpc");
    phy = iio_context_find_device(ctx, "ad9361-phy");
    bool is_fmcomms4 = !iio_device_find_channel(phy, "voltage1", false);
    if (!dev || !phy) {
            if (destroy_ctx)
                iio_context_destroy(ctx);
            throw std::runtime_error("Device not found");
        }
    
    // 2. channel_list initialize
    std::vector<std::string> channels = get_channels_vector(rx1_en, rx2_en);

    // 3. channel_list fill & enable
    nb_channels = iio_device_get_channels_count(dev);
    for (i = 0; i < nb_channels; i++)
        iio_channel_disable(iio_device_get_channel(dev, i));

    if (channels.empty()) {
        for (i = 0; i < nb_channels; i++) {
            struct iio_channel *chn =
                iio_device_get_channel(dev, i);

            iio_channel_enable(chn);
            channel_list.push_back(chn);
        }
    } else {
        for (std::vector<std::string>::const_iterator it =
                channels.begin();
                it != channels.end(); ++it) {
            struct iio_channel *chn =
                iio_device_find_channel(dev,
                        it->c_str(), false);
            if (!chn) {
                if (destroy_ctx)
                    iio_context_destroy(ctx);
                throw std::runtime_error(
                        "Channel not found");
            }

            iio_channel_enable(chn);
            channel_list.push_back(chn);
        }
    }
    
    // 4. calculate sample_bytes
    sample_bytes = iio_channel_get_data_format(channel_list[0])->length / 8;

    if(sample_bytes == 0)
        throw std::runtime_error("Invalid sample size");
    
    // 5. buffer_size check, buffer_size needs to meet the equation (buffer_size % sample_bytes == 0)
    // Because a pointer has to point exact memory.
    buffer_size = (buffer_size / sample_bytes) * sample_bytes;
    
    if(buffer_size == 0)
        throw std::runtime_error("buffer_size if too small after alignment");
    
    // 6. create buffer
    buf = iio_device_create_buffer(dev, buffer_size, false);
    if (!buf)
        throw std::runtime_error("Failed to create IIO buffer");

    samples_per_pdu = std::max<size_t>(1, static_cast<size_t>(std::round(samplerate * pdu_duration)));

    std::vector<std::string> params;

    if (!filter.empty())
        auto_filter = false;

    params.push_back("out_altvoltage0_RX_LO_frequency=" +
            std::to_string(static_cast<unsigned long long>(frequency)));
    params.push_back("in_voltage_sampling_frequency=" +
            std::to_string(samplerate));
    params.push_back("in_voltage_rf_bandwidth=" +
            std::to_string(bandwidth));
    params.push_back("in_voltage_quadrature_tracking_en=" +
            std::to_string(quadrature));
    params.push_back("in_voltage_rf_dc_offset_tracking_en=" +
            std::to_string(rfdc));
    params.push_back("in_voltage_bb_dc_offset_tracking_en=" +
            std::to_string(bbdc));
    std::string gain1_str = gain1;
    params.push_back("in_voltage0_gain_control_mode=" +
            gain1_str);
    if (gain1_str.compare("manual") == 0) {
            params.push_back("in_voltage0_hardwaregain=" +
            std::to_string(gain1_value));
    }
    if (!is_fmcomms4) {
        std::string gain2_str = gain2;
        params.push_back("in_voltage1_gain_control_mode=" +
                gain2_str);
        if (gain2_str.compare("manual") == 0) {
            params.push_back("in_voltage1_hardwaregain=" +
                    std::to_string(gain2_value));
            }
    }
    std::string rf_port_select = this->rf_port_select;
    params.push_back("in_voltage0_rf_port_select=" +
            rf_port_select);

    pluto_source_impl::set_params(phy, params);
    
    finished.store(false);
    rx_thread = std::thread(&pluto_source_impl::receive, this);
    return true;
}

bool pluto_source_impl::stop()
{
    finished.store(true);

    if (rx_thread.joinable()) {
        rx_thread.join();
    }
    if (buf) { iio_buffer_cancel(buf); iio_buffer_destroy(buf); buf = nullptr; }
    for (auto &ch : channel_list) iio_channel_disable(ch);
    channel_list.clear();
    if (destroy_ctx && ctx) { iio_context_destroy(ctx); ctx = nullptr; destroy_ctx = false; }

    return gr::block::stop();
}

void pluto_source_impl::receive() 
{
    int cnt = 0;
    long seq = 0;
    std::vector<std::complex<float>> pbuf;
    pbuf.reserve(samples_per_pdu > 0 ? samples_per_pdu : 1024);

    try {
        while (!finished.load()) {
            ssize_t ret = iio_buffer_refill(buf);
            if (ret < 0) {
                char errbuf[256];
                iio_strerror(-ret, errbuf, sizeof(errbuf));
                std::cerr << "[Pluto] iio_buffer_refill error: " << errbuf << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // 
            void* ptr_i = iio_buffer_first(buf, channel_list[0]);
            void* ptr_q = (channel_list.size() > 1) ? iio_buffer_first(buf, channel_list[1]) : nullptr;
            void* ptr_end = iio_buffer_end(buf);

            if (!ptr_i || !ptr_q || !ptr_end) {
                std::cerr << "[Pluto] invalid buffer pointers\n";
                continue;
            }

            uintptr_t src_i = reinterpret_cast<uintptr_t>(ptr_i);
            uintptr_t src_q = reinterpret_cast<uintptr_t>(ptr_q);
            uintptr_t end   = reinterpret_cast<uintptr_t>(ptr_end);

            size_t step_bytes = iio_buffer_step(buf);
            if (step_bytes == 0) {
                std::cerr << "[Pluto] step_bytes==0\n";
                continue;
            }

            size_t bytes_i = (src_i < end) ? (end - src_i) : 0;
            size_t bytes_q = (src_q < end) ? (end - src_q) : 0;
            size_t samples_i = bytes_i / step_bytes;
            size_t samples_q = bytes_q / step_bytes;
            size_t samples_available = std::min(samples_i, samples_q);

            size_t consumed = 0;
            while (samples_available > 0 && !finished.load()) {
                size_t take = std::min(samples_available, samples_per_pdu);
                pbuf.resize(take);
                for (size_t k = 0; k < take; ++k) {
                    short si = 0, sq = 0;
                    iio_channel_convert(channel_list[0], &si, reinterpret_cast<const void*>(src_i));
                    iio_channel_convert(channel_list[1], &sq, reinterpret_cast<const void*>(src_q));
                    pbuf[k] = std::complex<float>((float)si / 2048.0f, (float)sq / 2048.0f);
                    src_i += step_bytes; src_q += step_bytes;
                }
                samples_available -= take;
                consumed += take;

                // publish (수정된 블록)
                if (!pbuf.empty()) {
                    // 1) 평균 전력 계산 (complex<float>의 제곱 크기 평균)
                    double power = 0.0;
                    for (size_t i = 0; i < pbuf.size(); ++i) {
                        power += std::norm(pbuf[i]); // std::norm = re^2 + im^2
                    }
                    power /= static_cast<double>(pbuf.size()); // linear-domain mean power

                    // 2) 잡음바닥 초기화/적응 (EMA). 검출 상태일 때는 잡음 추정에 섞이지 않도록 비검출 때만 갱신
                    if (!d_noise_floor_initialized) {
                        d_noise_floor = power;
                        d_noise_floor_initialized = true;
                    }

                    double thresh_lin = std::pow(10.0, d_detect_threshold_db / 10.0); // dB -> 선형
                    bool detected = (power > d_noise_floor * thresh_lin);

                    if (!detected) {
                        // 비검출 구간에서만 잡음바닥 적응 (검출 구간을 포함하면 신호가 잡음추정에 섞일 수 있음)
                        d_noise_floor = (1.0 - d_noise_alpha) * d_noise_floor + d_noise_alpha * power;
                    }

                    // 3) 기존처럼 PDU 생성 및 메타에 detect 추가
                    pmt::pmt_t pdu = pmt::init_c32vector(
                        static_cast<int>(pbuf.size()),
                        reinterpret_cast<const gr_complex*>(pbuf.data()));

                    pmt::pmt_t meta = pmt::make_dict();
                    meta = pmt::dict_add(meta, pmt::intern("center_freq"), pmt::from_double(frequency));
                    meta = pmt::dict_add(meta, pmt::intern("seq"), pmt::from_long(seq++));

                    // elapsed time (us)
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - d_start_time).count();
                    meta = pmt::dict_add(meta, pmt::intern("timestamp"), pmt::from_double(static_cast<double>(elapsed_us)));

                    // detection metadata
                    meta = pmt::dict_add(meta, pmt::intern("detect"), pmt::from_long(detected ? 1 : 0));
                    meta = pmt::dict_add(meta, pmt::intern("power"), pmt::from_double(power));           // linear power
                    meta = pmt::dict_add(meta, pmt::intern("noise_floor"), pmt::from_double(d_noise_floor)); // linear

                    message_port_pub(pmt::mp("out"), pmt::cons(meta, pdu));
                }
            } // while samples_available
        } // while !finished
    } catch (const std::exception &ex) {
        std::cerr << "[Pluto] receive() exception: " << ex.what() << std::endl;
    } catch (...) {
        std::cerr << "[Pluto] receive() unknown exception\n";
    }
}

void pluto_source_impl::run()
{
    
}

void pluto_source_impl::set_metadata_keys(const std::string& txk,
                                             const std::string& rxk,
                                             const std::string& sk)
{
    tx_freq_key = txk;
    rx_freq_key = rxk;
    sample_start_key = sk;
}

} // namespace plasma
} // namespace gr