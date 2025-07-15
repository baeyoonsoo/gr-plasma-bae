// pluto_radar_bae_impl.cc
#include "pluto_radar_bae_impl.h"
#include <gnuradio/plasma/pluto_radar_bae.h>
#include <gnuradio/io_signature.h>

#include <gnuradio/blocks/short_to_float.h>
#include <gnuradio/blocks/float_to_complex.h>
#include <pmt/pmt.h>
#include <chrono>
#include <stdexcept>
#include <iio.h>


namespace gr {
namespace plasma {

pluto_radar_bae::sptr pluto_radar_bae::make(const std::string &uri,
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
                 bool auto_filter)
{
    return gnuradio::make_block_sptr<pluto_radar_bae_impl>(uri,
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
        auto_filter
    );
}

void pluto_radar_bae_impl::set_params(struct iio_device *phy,
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
pluto_radar_bae_impl::pluto_radar_bae_impl(const std::string &uri,
                 unsigned long long frequency,
                 unsigned long samplerate,
                 unsigned long bandwidth,
                 bool rx1_en, bool rx2_en,
                 unsigned long buffer_size,
                 bool quadrature, bool rfdc, bool bbdc,
                 const char *gain1, double gain1_value,
                 const char *gain2, double gain2_value,
                 const char *rf_port_select,
                 const char *filter = "",
                 bool auto_filter = true)
    : gr::block("pluto_radar_bae", gr::io_signature::make(0, 0, 0), gr::io_signature::make(0, 0, 0)),
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
      auto_filter(auto_filter)
{
    // message_port_register_in(PMT_IN);
    message_port_register_out(pmt::mp("out"));
    // set_msg_handler(PMT_IN, [this](const pmt::pmt_t& msg) { handle_message(msg); });
}

pluto_radar_bae_impl::~pluto_radar_bae_impl() {}

std::vector<std::string> pluto_radar_bae_impl::get_channels_vector(
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

bool pluto_radar_bae_impl::start()
{
    unsigned int nb_channels, i;
	unsigned short vid, pid;
    std::vector<std::string> channels = get_channels_vector(rx1_en, rx2_en);
    // IIO setup
    ctx = uri.empty() ? iio_create_default_context() : iio_create_context_from_uri(uri.c_str());
    if (!ctx)
		throw std::runtime_error("Unable to create context");
    destroy_ctx = true;
    dev = iio_context_find_device(ctx, "cf-ad9361-lpc");
    phy = iio_context_find_device(ctx, "ad9361-phy");
    buf = iio_device_create_buffer(dev, buffer_size, false);
    bool is_fmcomms4 = !iio_device_find_channel(phy, "voltage1", false);
    if (!dev || !phy) {
		    if (destroy_ctx)
			    iio_context_destroy(ctx);
		    throw std::runtime_error("Device not found");
	    }
    /* First disable all channels */
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
    
    std::vector<std::string> params;

    if (!filter.empty())
        auto_filter = false;

    params.push_back("out_altvoltage0_RX_LO_frequency=" +
            std::to_string(frequency));
    if (!auto_filter) {
        params.push_back("in_voltage_sampling_frequency=" +
                std::to_string(samplerate));
    }
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

    pluto_radar_bae_impl::set_params(phy, params);
    
    // you can use filter with this code but you need to change ad9361_set_bb_rate.
    // or you can connect FIR block provided by GNURadio.
    // if (auto_filter) {
    //     int ret = ad9361_set_bb_rate(phy, samplerate);
    //     if (ret) {
    //         throw std::runtime_error("Unable to set BB rate");
    //     }
    // } else if (!filter.empty()) {
    //     std::string filt(filter);
    //     if (!load_fir_filter(filt, phy))
    //         throw std::runtime_error("Unable to load filter file");
    // }
    // Conversion blocks
    // you can use later, not now
    // s2f_i = gr::blocks::short_to_float::make(1, 2048.0f);
    // s2f_q = gr::blocks::short_to_float::make(1, 2048.0f);
    // f2c   = gr::blocks::float_to_complex::make(1);
    // temp_i.resize(buffer_size);
    // temp_q.resize(buffer_size);

    finished = false;
    refill_thread = std::thread([this](){
    ssize_t ret;
    std::unique_lock<std::mutex> lock(refill_mtx);
    while (!finished) {
        // wait refill request
        refill_cv.wait(lock, [this]{ return please_refill || finished; });
        if (finished) break;
        please_refill = false;
        lock.unlock();

        // try refill
        ret = iio_buffer_refill(buf);
        if (ret < 0) {
            // error log except -EBADF
            char errbuf[256];
            iio_strerror(-ret, errbuf, sizeof(errbuf));
            std::cerr << "Unable to refill buffer: " << errbuf << std::endl;
            break;
        }

        // rx thread ready call
        lock.lock();
        {
            std::lock_guard<std::mutex> lk(mtx);
            data_ready = true;
        }
        cv.notify_one();
    }
    // stop thread
    thread_stopped = true;
    cv.notify_all();
    });
    rx_thread = std::thread(&pluto_radar_bae_impl::run, this);
    return true;
}

bool pluto_radar_bae_impl::stop()
{
    finished = true;
    {
      std::lock_guard<std::mutex> lk(refill_mtx);
      please_refill = true;
    }
    refill_cv.notify_all();
    cv.notify_all();
    if (refill_thread.joinable()) refill_thread.join();
    if (rx_thread.joinable()) rx_thread.join();
    if (buf) { iio_buffer_destroy(buf); buf = nullptr; }
    if (ctx && destroy_ctx) { iio_context_destroy(ctx); ctx = nullptr; }
    return true;
}

// void pluto_radar_bae_impl::handle_message(const pmt::pmt_t& msg)
// {
//     // Processing valid pdu 
//     if (pmt::is_pdu(msg)) {
//         next_meta = pmt::dict_update(next_meta, pmt::car(msg));
//         tx_data = pmt::cdr(msg);
//         tx_buff_size = pmt::length(tx_data);

//         new_msg_received = true;
//     }
// }

void pluto_radar_bae_impl::run()
{
    const int decimation = 0;
    const int timeout_ms = 1000;

    size_t sample_bytes = iio_channel_get_data_format(channel_list[0])->length / 8;
    size_t step_bytes = iio_buffer_step(buf) * (decimation + 1);
    size_t nelem = buffer_size / (step_bytes * 2);

    std::vector<std::complex<float>> cplx_buf(nelem);
    std::vector<short> raw_i(nelem), raw_q(nelem);
       
    while (!finished) {
        {
            std::lock_guard<std::mutex> lk(refill_mtx);
            please_refill = true;
        }
        refill_cv.notify_one();
        std::unique_lock<std::mutex> lk(mtx);
        if (!cv.wait_for(lk, std::chrono::milliseconds(timeout_ms), [this]{ return data_ready; })) {
            std::cerr << "[Pluto] Timeout waiting for buffer refill\n";
            continue;
        }
        
        //cv.wait(lk, [this]{ return data_ready; });
        data_ready = false;
        if (finished) return;

        // read channel
        void* ptr_i = /*(void*)*/ iio_buffer_first(buf, /*iio_device_find_channel(dev, "voltage0", false)*/channel_list[0]);
        void* ptr_q = /*(void*)*/ iio_buffer_first(buf, /*iio_device_find_channel(dev, "voltage1", false)*/channel_list[1]);
        uintptr_t src_i = (uintptr_t)ptr_i;
        uintptr_t src_q = (uintptr_t)ptr_q;
        uintptr_t end   = (uintptr_t)iio_buffer_end(buf);

        size_t idx = 0;
        while (src_i < end && src_q < end && idx < nelem) {
            short s_i, s_q;
            iio_channel_convert(channel_list[0], &s_i, (const void*)src_i);
            iio_channel_convert(channel_list[1], &s_q, (const void*)src_q);

            cplx_buf[idx] = std::complex<float>((float)s_i / 2048.0f, (float)s_q / 2048.0f);

            src_i += step_bytes;
            src_q += step_bytes;
            ++idx;
        }

        if (idx == 0) {
            std::cerr << "[Pluto] No samples processed, skipping publish\n";
            continue;
        }

        // // short->float
        // s2f_i->work(1, {ptr_i}, {temp_i.data()});
        // s2f_q->work(1, {ptr_q}, {temp_q.data()});

        // // float->complex
        // f2c->work(1,
        //           {temp_i.data(), temp_q.data()},
        //           {out_ptr});

        // add metadata
        pmt::pmt_t pdu = pmt::init_c32vector(idx, cplx_buf.data());
        pmt::pmt_t meta = pmt::make_dict();
        meta = pmt::dict_add(meta, pmt::intern(rx_freq_key), pmt::from_double(frequency));
        message_port_pub(pmt::mp("out"), pmt::cons(meta, pdu));
        
    }
}

void pluto_radar_bae_impl::set_metadata_keys(const std::string& txk,
                                             const std::string& rxk,
                                             const std::string& sk)
{
    tx_freq_key = txk;
    rx_freq_key = rxk;
    sample_start_key = sk;
}

} // namespace plasma
} // namespace gr