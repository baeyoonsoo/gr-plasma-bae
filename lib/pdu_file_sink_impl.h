/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_PLASMA_PDU_FILE_SINK_IMPL_H
#define INCLUDED_PLASMA_PDU_FILE_SINK_IMPL_H

#include <gnuradio/plasma/pdu_file_sink.h>
#include <gnuradio/plasma/pmt_constants.h>
#include <nlohmann/json.hpp>
#include <uhd/utils/thread.hpp>
#include <fstream>
#include <queue>

namespace gr {
namespace plasma {

class pdu_file_sink_impl : public pdu_file_sink
{
private:

    size_t d_itemsize;
    std::string d_data_filename;
    std::string d_meta_filename;
    std::queue<pmt::pmt_t> d_data_queue;
    std::queue<pmt::pmt_t> d_meta_queue;
    std::ofstream d_data_file;
    std::ofstream d_meta_file;
    bool detected_only;
    gr::thread::thread d_thread;
    gr::thread::mutex d_mutex;
    gr::thread::condition_variable d_cond;
    std::atomic<bool> d_finished;
    pmt::pmt_t d_data;
    pmt::pmt_t d_meta_dict;
    nlohmann::json d_meta;
    nlohmann::json d_global;
    nlohmann::json d_capture;
    nlohmann::json d_annotation;
    std::string get_datatype_string();
    void parse_meta(const pmt::pmt_t& dict, nlohmann::json& json);

public:
    pdu_file_sink_impl(size_t itemsize,
                       std::string& data_filename,
                       std::string& meta_filename,
                       bool detected_only);
    ~pdu_file_sink_impl();
    
    void handle_message(const pmt::pmt_t& msg);

    bool start() override;
    bool stop() override;
    void run();
};

} // namespace plasma
} // namespace gr

#endif /* INCLUDED_PLASMA_PDU_FILE_SINK_IMPL_H */
