/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_PLASMA_SIGNAL_PROCESSING_IMPL_H
#define INCLUDED_PLASMA_SIGNAL_PROCESSING_IMPL_H

#include <gnuradio/plasma/signal_processing.h>
#include <gnuradio/plasma/pmt_constants.h>
#include <arrayfire.h>
#include <vector>
#include <pmt/pmt.h>
#include <string>

namespace gr {
namespace plasma {

class signal_processing_impl : public signal_processing
{
private:
    /* --- 주요 설정/상태 --- */
    size_t fft_size;            // 사용자 설정 FFT 길이
    double sample_rate;         // 샘플레이트 (Hz)
    size_t n_samples;           // 처리 청크 길이 (sample_rate * 100us)
    bool fft_on;                // FFT 수행 온/오프

    /* 출력 형식 */
    bool output_magnitude;      // true -> output magnitude(real float), false -> complex c32

    /* FFT 성능 옵션 */
    bool use_fft_plan_cache;    // plan cache 사용 여부
    bool use_inplace_fft;       // in-place FFT 사용 여부 (n_samples == fft_size 인 경우 권장)
    size_t fft_plan_cache_size; // plan cache 크기 (정수)

    /* 윈도우 & 오버랩 */
    int window_type;            // WINDOW_NONE / WINDOW_HANN / WINDOW_HAMMING (정수)
    double overlap_frac;        // 오버랩 비율 (0.0 .. <1.0)
    size_t overlap_samples;     // 오버랩 샘플 수 (internal)
    std::vector<gr_complex> overlap_buffer; // 오버랩/버퍼링을 위한 호스트 버퍼
    bool process_partials;      // 남은 미완성 청크를 즉시 제로패드 처리할지 여부
    
    size_t d_msg_queue_depth;

    pmt::pmt_t d_meta;
    pmt::pmt_t d_data;
    pmt::pmt_t d_rx_port;
    pmt::pmt_t d_time_port;
    pmt::pmt_t d_freq_port;
    
    void handle_rx_msg(pmt::pmt_t msg);

public:
    signal_processing_impl(size_t fft_size, double sample_rate, bool fft_on, int output_magnitude, int window_type, float overlap_frac);
    ~signal_processing_impl();

    void set_msg_queue_depth(size_t depth) override;
    void set_backend(Device::Backend backend) override;
    void set_metadata_keys(const std::string& n_pulse_cpi_key) override;
};

} // namespace plasma
} // namespace gr

#endif /* INCLUDED_PLASMA_SIGNAL_PROCESSING_IMPL_H */
