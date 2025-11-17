/* -*- c++ -*- */
/*
 * Copyright 2022 gr-plasma author.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <Python.h>
#include "spectro_sink_impl.h"
#include <gnuradio/io_signature.h>
#include <QApplication>
#include <chrono>
#include <thread>

namespace gr {
namespace plasma {

spectro_sink::sptr spectro_sink::make(double samp_rate,
                              int fft_size,
                              size_t ncol,
                              double center_freq,
                              QWidget* parent)
{
    return gnuradio::make_block_sptr<spectro_sink_impl>(
        samp_rate, fft_size, ncol, center_freq, parent);
}

/*
 * The private constructor
 */
spectro_sink_impl::spectro_sink_impl(double samp_rate,
                             int fft_size,
                             size_t ncol,
                             double center_freq,
                             QWidget* parent)
    : gr::block("spectro_sink",
              gr::io_signature::make(0,0,0),
              gr::io_signature::make(0,0,0)),
    d_samp_rate(samp_rate),
    d_fft_size(fft_size),   // 선언 순서와 맞춤
    d_ncol(ncol),
    d_center_freq(center_freq)
{
    if (const char* p = std::getenv("PLASMA_DB_PATH"))   d_db_path   = p;
    if (const char* d = std::getenv("PLASMA_DEVICE_ID")) d_device_id = d;
    if (const char* en = std::getenv("PLASMA_DB_ENABLE")) {
        d_db_enable = (std::string(en) == "1" || std::string(en) == "true");
    }
    try {
        db_open_and_prepare();
        d_db_thread = std::thread(&spectro_sink_impl::db_thread_loop, this);
    } catch (const std::exception& e) {
        std::cerr << "[spectro_sink] SQLite init failed: " << e.what()
                  << " (DB disabled)\n";
        d_db_enable = false;
    }

    // Initialize the QApplication
    static int argc = 1;
    static char app[] = "spectro";
    static char* argv[] = { app, nullptr };

    if (qApp)
    d_qapp = qApp;
    else
    d_qapp = new QApplication(argc, argv);
        
    d_main_gui = new SpectroWindow(parent, samp_rate, center_freq);

    // time set
    set_update_time(0.1);

    // Initialize message ports
    d_in_port = PMT_IN;
    message_port_register_in(d_in_port);
    set_msg_handler(d_in_port, [this](pmt::pmt_t msg) { handle_rx_msg(msg); });
}

/*
 * Our virtual destructor.
 */
spectro_sink_impl::~spectro_sink_impl() {
    if (d_db_enable) {
        { std::lock_guard<std::mutex> lk(d_m); d_stop = true; }
        d_cv.notify_all();
        if (d_db_thread.joinable()) d_db_thread.join();
        db_close();
    }
}

bool spectro_sink_impl::start()
{
    d_finished = false;
    return block::start();
}

bool spectro_sink_impl::stop()
{
    d_finished = true;
    if (not d_main_gui->is_closed())
        d_main_gui->close();
    return block::stop();
}

void spectro_sink_impl::exec_() { d_qapp->exec(); };

QWidget* spectro_sink_impl::qwidget() { return (QWidget*)d_main_gui; }

#ifdef ENABLE_PYTHON
PyObject* spectro_sink_impl::pyqwidget()
{
    PyObject* w = PyLong_FromVoidPtr((void*)d_main_gui);
    PyObject* retarg = Py_BuildValue("N", w);
    return retarg;
}
#else
void* spectro_sink_impl::pyqwidget() { return nullptr; }
#endif

void spectro_sink_impl::handle_rx_msg(pmt::pmt_t msg)
{
    if (d_main_gui->busy() or this->nmsgs(d_in_port) > d_msg_queue_depth) {
        return;
    }
    pmt::pmt_t samples;
    if (pmt::is_pdu(msg)) {
        samples = pmt::cdr(msg);
        d_meta = pmt::car(msg);
    } else if (pmt::is_uniform_vector(msg)) {
        samples = msg;
    } else {
        return;
    }

    size_t len = pmt::length(samples);
    if (len==0) return;

    const size_t ncol = len;
    std::vector<double> cur(len);

    if (pmt::is_f32vector(samples)) {
        const float* in = pmt::f32vector_elements(samples, len);
        for (size_t i = 0; i < len; ++i) cur[i] = static_cast<double>(in[i]);
    } else if (pmt::is_c32vector(samples)){
        const gr_complex* in = pmt::c32vector_elements(samples, len);
        for (size_t i = 0; i < len; ++i) cur[i] = static_cast<double>(std::abs(in[i]));
    } else {
        return;
    }

    // meta
    size_t N = 1024;
    d_fft_size = pmt::to_long(pmt::dict_ref(d_meta, pmt::intern("fft_size"), pmt::from_long(static_cast<long>(N))));
    d_samp_rate = pmt::to_double(pmt::dict_ref(d_meta, pmt::intern("samp_rate"), pmt::from_double(d_samp_rate)));
    d_center_freq = pmt::to_double(pmt::dict_ref(d_meta, pmt::intern("center_freq"), pmt::from_double(d_center_freq)));

    // ===== 평균 누산 =====
    // FFT 크기가 바뀌면 누산기 리셋
    if (d_accum_cols != ncol || d_accum_buf.size() != ncol) {
        d_accum_cols  = ncol;
        d_accum_buf.assign(ncol, 0.0);
        d_accum_count = 0;
    }
    // 합계 누산
    for (size_t i = 0; i < ncol; ++i) d_accum_buf[i] += cur[i];
    d_accum_count += 1;

    // ===== 타이머 체크 & 주기 갱신 =====
    const auto now_ticks = gr::high_res_timer_now();
    if (now_ticks - d_last_time >= d_update_time && d_accum_count > 0) {
        d_last_time = now_ticks;

        // 평균 계산
        std::vector<double> avg(d_accum_cols);
        const double inv = 1.0 / static_cast<double>(d_accum_count);
        for (size_t i = 0; i < d_accum_cols; ++i) avg[i] = d_accum_buf[i] * inv;

        // 시간 정보
        const auto tps = gr::high_res_timer_tps();
        const uint64_t now_us    = static_cast<uint64_t>((now_ticks * 1000000.0) / tps);

        // 실제 이벤트 간 간격(측정치)
        double frame_period_s;
        if (d_have_emit_tick) {
            const uint64_t dt_ticks = now_ticks - d_last_emit_ticks;
            frame_period_s = static_cast<double>(dt_ticks) / static_cast<double>(tps);
        } else {
            // 첫 이벤트는 설정값으로 초기화
            frame_period_s = d_update_sec;
            d_have_emit_tick = true;
        }
        d_last_emit_ticks = now_ticks;

        set_time_per_fft(frame_period_s);

        // 이벤트 발송
        d_qapp->postEvent(d_main_gui,
            new SpectroUpdateEvent(avg.data(), /*rows*/1, d_accum_cols, d_meta, now_us, frame_period_s));
        
        // DB로도 동일 프레임 전송
        if (d_db_enable) {
            frame_row r;
            r.ts_us        = now_us;
            r.device_id    = d_device_id;              // "pluto-01" 같은 값
            r.center_hz    = d_center_freq;
            r.samp_rate_hz = d_samp_rate;
            r.fft_size     = static_cast<int>(d_accum_cols);
            r.bin0_hz      = r.center_hz - r.samp_rate_hz/2.0;
            r.df_hz        = r.samp_rate_hz / r.fft_size;

            r.power_db.resize(d_accum_cols);
            for (size_t i = 0; i < d_accum_cols; ++i)
                r.power_db[i] = static_cast<float>(avg[i]); // float32로 저장

            enqueue_frame(std::move(r));
        }

        // 누산기 리셋
        std::fill(d_accum_buf.begin(), d_accum_buf.end(), 0.0);
        d_accum_count = 0;
    }
}

void spectro_sink_impl::set_update_time(double t)
{
    // convert update time to ticks
    gr::high_res_timer_type tps = gr::high_res_timer_tps();
    d_update_time = t * tps;
    d_main_gui->setUpdateTime(t);
    d_update_sec  = t;
    d_last_time = 0;
}

void spectro_sink_impl::set_msg_queue_depth(size_t depth)
{
    d_msg_queue_depth = depth;
}

void spectro_sink_impl::set_metadata_keys(const std::string& samp_rate_key,
                                            const std::string& n_matrix_col_key,
                                            const std::string& center_freq_key)
{
    d_samp_rate_key     = pmt::intern(samp_rate_key);
    d_n_matrix_col_key  = pmt::intern(n_matrix_col_key);
    d_center_freq_key   = pmt::intern(center_freq_key);
    d_main_gui->set_metadata_keys(samp_rate_key, n_matrix_col_key, center_freq_key);
    // throw std::runtime_error("1\n");
}

void spectro_sink_impl::set_time_per_fft(double t) { 
    d_main_gui->setUpdateTime(t); 
}

long long spectro_sink_impl::now_us() const {
  using namespace std::chrono;
  return duration_cast<microseconds>(
           std::chrono::system_clock::now().time_since_epoch()).count();
}

void spectro_sink_impl::db_open_and_prepare() {
  if (sqlite3_open_v2(d_db_path.c_str(), &d_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
    throw std::runtime_error("sqlite open failed");

  sqlite3_exec(d_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
  sqlite3_exec(d_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

  const char* ddl =
    "CREATE TABLE IF NOT EXISTS spectrum_frames("
    " ts_us INTEGER NOT NULL, device_id TEXT NOT NULL,"
    " center_hz REAL NOT NULL, samp_rate_hz REAL NOT NULL,"
    " fft_size INTEGER NOT NULL, bin0_hz REAL NOT NULL, df_hz REAL NOT NULL,"
    " power_db BLOB NOT NULL );"
    "CREATE INDEX IF NOT EXISTS idx_frames_ts ON spectrum_frames(ts_us DESC);"
    "CREATE INDEX IF NOT EXISTS idx_frames_dev_ts ON spectrum_frames(device_id, ts_us DESC);";
  sqlite3_exec(d_db, "BEGIN;", nullptr, nullptr, nullptr);
  sqlite3_exec(d_db, ddl,       nullptr, nullptr, nullptr);
  sqlite3_exec(d_db, "COMMIT;", nullptr, nullptr, nullptr);

  const char* SQL =
    "INSERT INTO spectrum_frames"
    " (ts_us, device_id, center_hz, samp_rate_hz, fft_size, bin0_hz, df_hz, power_db)"
    " VALUES (?,?,?,?,?,?,?,?);";
  if (sqlite3_prepare_v2(d_db, SQL, -1, &d_stmt, nullptr) != SQLITE_OK)
    throw std::runtime_error("sqlite prepare failed");
}

void spectro_sink_impl::db_thread_loop() {
  const int BATCH = 20;
  std::vector<frame_row> batch; batch.reserve(BATCH);

  for (;;) {
    {
      std::unique_lock<std::mutex> lk(d_m);
      d_cv.wait(lk, [&]{ return d_stop || !d_q.empty(); });
      if (d_stop && d_q.empty()) break;
      while (!d_q.empty() && (int)batch.size() < BATCH) {
        batch.emplace_back(std::move(d_q.front()));
        d_q.pop();
      }
    }
    if (batch.empty()) continue;

    sqlite3_exec(d_db, "BEGIN;", nullptr, nullptr, nullptr);
    for (auto& r : batch) {
      sqlite3_reset(d_stmt); sqlite3_clear_bindings(d_stmt);
      sqlite3_bind_int64 (d_stmt, 1, r.ts_us);
      sqlite3_bind_text  (d_stmt, 2, r.device_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_double(d_stmt, 3, r.center_hz);
      sqlite3_bind_double(d_stmt, 4, r.samp_rate_hz);
      sqlite3_bind_int   (d_stmt, 5, r.fft_size);
      sqlite3_bind_double(d_stmt, 6, r.bin0_hz);
      sqlite3_bind_double(d_stmt, 7, r.df_hz);
      sqlite3_bind_blob  (d_stmt, 8, r.power_db.data(),
                          (int)(r.power_db.size()*sizeof(float)), SQLITE_TRANSIENT);
      (void)sqlite3_step(d_stmt);
    }
    sqlite3_exec(d_db, "COMMIT;", nullptr, nullptr, nullptr);
    batch.clear();
  }
}

void spectro_sink_impl::db_close() {
  if (d_stmt) { sqlite3_finalize(d_stmt); d_stmt = nullptr; }
  if (d_db)   { sqlite3_close(d_db);      d_db   = nullptr; }
}

void spectro_sink_impl::enqueue_frame(frame_row&& r) {
  std::lock_guard<std::mutex> lk(d_m);
  d_q.emplace(std::move(r));
  d_cv.notify_one();
}

void spectro_sink_impl::set_db_enable(bool enable)
{
    d_db_enable = enable;
}
void spectro_sink_impl::set_db_path(const std::string& path)
{
    if (path.empty() || path == d_db_path) return;
    d_db_path = path;
}

void spectro_sink_impl::set_device_id(const std::string& id)
{
    if (!id.empty()) d_device_id = id;
}

} /* namespace plasma */
} /* namespace gr */