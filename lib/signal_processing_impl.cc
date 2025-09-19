/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "signal_processing_impl.h"
#include <gnuradio/io_signature.h>
#include <arrayfire.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <iostream> // for std::cerr

namespace gr {
namespace plasma {

// Window types
static const int WINDOW_NONE = 0;
static const int WINDOW_HANN = 1;
static const int WINDOW_HAMMING = 2;

signal_processing::sptr signal_processing::make(size_t fft_size,
                                                double sample_rate,
                                                bool fft_on,
                                                int output_magnitude,
                                                int window_type
                                                )
{
    return gnuradio::make_block_sptr<signal_processing_impl>(fft_size, sample_rate, fft_on, output_magnitude, window_type);
}


/*
 * The private constructor
 */
signal_processing_impl::signal_processing_impl(size_t fft_size,
                                               double sample_rate,
                                               bool fft_on,
                                               int output_magnitude,
                                               int window_type)
    : gr::block(
          "signal_processing", gr::io_signature::make(0, 0, 0), gr::io_signature::make(0, 0, 0)),
      fft_size(fft_size),
      sample_rate(sample_rate),
      fft_on(fft_on),
      d_msg_queue_depth(512),
      output_magnitude(output_magnitude),
      use_fft_plan_cache(false),
      use_inplace_fft(false),
      fft_plan_cache_size(8),
      window_type(window_type),
      overlap_frac(0.0),
      overlap_samples(0),
      process_partials(false)
{
    // compute n_samples = sample_rate * 100us
    if (sample_rate <= 0.0) {
        this->n_samples = 1; // fallback
    } else {
        this->n_samples = fft_size;
        if (this->n_samples == 0) this->n_samples = 1;
    }

    // compute overlap_samples based on overlap_frac
    this->overlap_samples = static_cast<size_t>(std::floor(this->overlap_frac * this->n_samples));
    if (this->overlap_samples >= this->n_samples)
        this->overlap_samples = this->n_samples > 1 ? this->n_samples - 1 : 0;

    d_data = pmt::make_c32vector(1, 0); // placeholder
    d_meta = pmt::make_dict();

    // Only RX and OUT ports
    d_rx_port = PMT_RX;
    d_time_port = pmt::mp("time");
    d_freq_port = pmt::mp("freq");

    message_port_register_in(d_rx_port);
    message_port_register_out(d_time_port);
    message_port_register_out(d_freq_port);
    set_msg_handler(d_rx_port, [this](pmt::pmt_t msg) { handle_rx_msg(msg); });

    // initialize ArrayFire fft plan cache if requested (can be toggled later)
    if (use_fft_plan_cache) {
        af::setFFTPlanCacheSize(static_cast<int>(fft_plan_cache_size));
    }
}

/*
 * Our virtual destructor.
 */
signal_processing_impl::~signal_processing_impl() {}

static std::vector<float> make_window(size_t N, int wtype)
{
    std::vector<float> w(N, 1.0f);
    if (wtype == 0 || N == 0) {
        return w;
    }
    if (wtype == 1) {
        // Hann: 0.5 * (1 - cos(2*pi*n/(N-1)))
        for (size_t n = 0; n < N; ++n) {
            w[n] = 0.5f * (1.0f - std::cos(2.0f * M_PI * static_cast<float>(n) / static_cast<float>(N - 1)));
        }
    } else if (wtype == 2) {
        // Hamming: 0.54 - 0.46 cos(2*pi*n/(N-1))
        for (size_t n = 0; n < N; ++n) {
            w[n] = 0.54f - 0.46f * std::cos(2.0f * M_PI * static_cast<float>(n) / static_cast<float>(N - 1));
        }
    }
    return w;
}

static af::array af_fftshift1d(const af::array &x)
{
    af::dim4 d = x.dims();
    const dim_t N = d[0];
    const dim_t shift = N / 2; // integer division (floor)

    if (N <= 1 || shift == 0) {
        return x; // nothing to do
    }

    // a = x[shift : N-1], b = x[0 : shift-1]
    af::array a = x(af::seq(shift, N - 1));
    af::array b = x(af::seq(0, shift - 1));
    return af::join(0, a, b);
}

void signal_processing_impl::handle_rx_msg(pmt::pmt_t msg)
{
    // guard: avoid piling up too many unprocessed messages
    if (this->nmsgs(d_rx_port) > d_msg_queue_depth) {
        return;
    }

    pmt::pmt_t samples, meta;
    if (pmt::is_pdu(msg)) {
        meta = pmt::car(msg);
        samples = pmt::cdr(msg);
        // Update/keep metadata
        d_meta = pmt::dict_update(d_meta, meta);
    } else if (pmt::is_uniform_vector(msg)) {
        samples = msg;
    } else {
        //GR_LOG_WARN(d_logger, "Invalid message type");
        std::cerr << "[signal_processing] WARN: Invalid message type\n";
        return;
    }

    size_t total_n = pmt::length(samples);
    if (total_n == 0) return;

    size_t io = 0;
    const gr_complex* in_ptr = pmt::c32vector_elements(samples, io);

    // Append incoming samples into overlap_buffer
    overlap_buffer.insert(overlap_buffer.end(), in_ptr, in_ptr + total_n);

    // recompute overlap_samples/hop in case n_samples or overlap_frac changed earlier
    if (overlap_frac <= 0.0) {
        overlap_samples = 0;
    } else {
        overlap_samples = static_cast<size_t>(std::floor(overlap_frac * static_cast<double>(n_samples)));
        if (overlap_samples >= n_samples)
            overlap_samples = n_samples > 1 ? n_samples - 1 : 0;
    }
    const size_t hop = (n_samples > overlap_samples) ? (n_samples - overlap_samples) : n_samples;

    // prepare window if needed
    af::array win_af; // empty by default
    bool use_window = (window_type != WINDOW_NONE);
    if (use_window) {
        std::vector<float> w = make_window(n_samples, window_type);
        // create af::array from host floats
        win_af = af::array(af::dim4(n_samples), w.data());
    }

    // Option: set fft plan cache when requested (can be toggled at runtime)
    if (use_fft_plan_cache) {
        af::setFFTPlanCacheSize(static_cast<int>(fft_plan_cache_size));
    }

    // Process while we have enough samples for a full chunk
    size_t buf_pos = 0;
    while (overlap_buffer.size() - buf_pos >= n_samples) {
        // build chunk of length n_samples from buffer starting at buf_pos
        // (AF expects column/vector)
        try {
            // Create af::array x of length n_samples from host memory (pointer into overlap_buffer)
            const gr_complex* chunk_ptr = overlap_buffer.data() + buf_pos;
            af::array x(af::dim4(n_samples), reinterpret_cast<const af::cfloat*>(chunk_ptr));

            // apply window if requested (broadcast multiply)
            if (use_window) {
                x = x * win_af;
            }

            // Decide whether to run FFT or pass-through time-domain
            if (fft_on) {
                af::array X; // result (complex) length fft_size (dim0)
                bool used_inplace = false;

                if (use_inplace_fft && n_samples == fft_size) {
                    // in-place FFT (사용하는 ArrayFire 버전에 따라 함수명이 다를 수 있음)
                    af::array x_copy = x; // 소유권/수정 가능하게 복사
                    // 예: af::fftInPlace(x_copy); 또는 af::fft_inplace(x_copy);
                    af::fftInPlace(x_copy);
                    X = x_copy;
                    used_inplace = true;
                } else {
                    // out-of-place FFT with explicit output length (zero-pad/truncate)
                    X = af::fft(x, static_cast<int>(fft_size)); // 1D FFT
                }

                af::array Xs = af_fftshift1d(X);

                if (output_magnitude) {
                    // compute magnitude (abs) => real float vector of length fft_size
                    //af::array mag = af::abs(Xs); // float array
                    af::array amp_linear = af::abs(Xs) / static_cast<float>(fft_size);
                    af::array mag = 20.0f * af::log10(amp_linear + 1e-20f);
                    pmt::pmt_t out_pdu = pmt::make_f32vector(static_cast<int>(mag.elements()), 0.0f);
                    size_t io_out = 0;
                    float* out_ptr = pmt::f32vector_writable_elements(out_pdu, io_out);
                    mag.host(out_ptr);
                    pmt::pmt_t meta_freq = pmt::dict_add(d_meta, pmt::intern("domain"), pmt::intern("freq"));
                    message_port_pub(d_freq_port, pmt::cons(meta_freq, out_pdu));
                } else {
                    // output complex spectrum as c32vector of length fft_size
                    pmt::pmt_t out_pdu = pmt::make_c32vector(static_cast<int>(fft_size), gr_complex(0, 0));
                    size_t io_out = 0;
                    gr_complex* out_ptr = pmt::c32vector_writable_elements(out_pdu, io_out);
                    Xs.host(reinterpret_cast<af::cfloat*>(out_ptr));
                    pmt::pmt_t meta_freq = pmt::dict_add(d_meta, pmt::intern("domain"), pmt::intern("freq"));
                    message_port_pub(d_freq_port, pmt::cons(meta_freq, out_pdu));
                }
            } else {
                // fft_on == false: time-domain pass-through for this chunk (length n_samples)
                if (output_magnitude) {
                    af::array mag = af::abs(x);
                    pmt::pmt_t out_pdu = pmt::make_f32vector(static_cast<int>(mag.elements()), 0.0f);
                    size_t io_out = 0;
                    float* out_ptr = pmt::f32vector_writable_elements(out_pdu, io_out);
                    mag.host(out_ptr);
                    pmt::pmt_t meta_time = pmt::dict_add(d_meta, pmt::intern("domain"), pmt::intern("time"));
                    message_port_pub(d_time_port, pmt::cons(meta_time, out_pdu));
                } else {
                    pmt::pmt_t out_pdu = pmt::make_c32vector(static_cast<int>(n_samples), gr_complex(0, 0));
                    size_t io_out = 0;
                    gr_complex* out_ptr = pmt::c32vector_writable_elements(out_pdu, io_out);
                    std::memcpy(out_ptr, chunk_ptr, sizeof(gr_complex) * n_samples);
                    pmt::pmt_t meta_time = pmt::dict_add(d_meta, pmt::intern("domain"), pmt::intern("time"));
                    message_port_pub(d_time_port, pmt::cons(meta_time, out_pdu));
                }
            }
        } catch (const af::exception& e) {
            //GR_LOG_ERROR(d_logger, std::string("ArrayFire error in handle_rx_msg: ") + e.what());
            std::cerr << "[signal_processing] ERROR: ArrayFire error in handle_rx_msg: " << e.what() << "\n";
            // On AF error, abort processing loop to avoid infinite tries
            break;
        }

        // advance buffer position by hop
        buf_pos += hop;
    } // end while processing chunks

    // remove consumed samples from overlap_buffer (keep remainder)
    if (buf_pos > 0) {
        overlap_buffer.erase(overlap_buffer.begin(), overlap_buffer.begin() + buf_pos);
    }
}

/* implement set_metadata_keys to satisfy abstract interface */
void signal_processing_impl::set_metadata_keys(const std::string& n_pulse_cpi_key)
{
    // For compatibility with previous interface, store the key as PMT symbol
    // If you need to use it, add a member to hold the interned key.
    // Currently we keep it as a no-op / placeholder.
    (void)n_pulse_cpi_key;
}

void signal_processing_impl::set_msg_queue_depth(size_t depth)
{
    d_msg_queue_depth = depth;
}

void signal_processing_impl::set_backend(Device::Backend backend)
{
    af::Backend b = AF_BACKEND_DEFAULT;
    switch (backend) {
    case Device::CPU:
        b = AF_BACKEND_CPU;
        break;
    case Device::CUDA:
        b = AF_BACKEND_CUDA;
        break;
    case Device::OPENCL:
        b = AF_BACKEND_OPENCL;
        break;
    default:
        b = AF_BACKEND_DEFAULT;
        break;
    }
    af::setBackend(b);
}

} /* namespace plasma */
} /* namespace gr */