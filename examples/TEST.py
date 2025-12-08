#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# SPDX-License-Identifier: GPL-3.0
#
# GNU Radio Python Flow Graph
# Title: TEST
# GNU Radio version: 3.10.9.2

from PyQt5 import Qt
from gnuradio import qtgui
from PyQt5 import Qt
from gnuradio import plasma
import sip
from gnuradio import gr
from gnuradio.filter import firdes
from gnuradio.fft import window
import sys
import signal
from argparse import ArgumentParser
from gnuradio.eng_arg import eng_float, intx
from gnuradio import eng_notation



class TEST(gr.top_block, Qt.QWidget):

    def __init__(self):
        gr.top_block.__init__(self, "TEST", catch_exceptions=True)
        Qt.QWidget.__init__(self)
        self.setWindowTitle("TEST")
        qtgui.util.check_set_qss()
        try:
            self.setWindowIcon(Qt.QIcon.fromTheme('gnuradio-grc'))
        except BaseException as exc:
            print(f"Qt GUI: Could not set Icon: {str(exc)}", file=sys.stderr)
        self.top_scroll_layout = Qt.QVBoxLayout()
        self.setLayout(self.top_scroll_layout)
        self.top_scroll = Qt.QScrollArea()
        self.top_scroll.setFrameStyle(Qt.QFrame.NoFrame)
        self.top_scroll_layout.addWidget(self.top_scroll)
        self.top_scroll.setWidgetResizable(True)
        self.top_widget = Qt.QWidget()
        self.top_scroll.setWidget(self.top_widget)
        self.top_layout = Qt.QVBoxLayout(self.top_widget)
        self.top_grid_layout = Qt.QGridLayout()
        self.top_layout.addLayout(self.top_grid_layout)

        self.settings = Qt.QSettings("GNU Radio", "TEST")

        try:
            geometry = self.settings.value("geometry")
            if geometry:
                self.restoreGeometry(geometry)
        except BaseException as exc:
            print(f"Qt GUI: Could not restore geometry: {str(exc)}", file=sys.stderr)

        ##################################################
        # Variables
        ##################################################
        self.samp_rate = samp_rate = 20000000
        self.center_freq = center_freq = 2400000000

        ##################################################
        # Blocks
        ##################################################

        self.pluto_source_0_0_0_0 = plasma.pluto_source(
          "ip:192.168.3.3",
          center_freq,
          samp_rate,
          1500000,
          True,
          True,
          4096,
          True,
          True,
          True,
          "manual",
          10.0,
          "manual",
          10.0,
          "A_BALANCED",
          -30.0,
          "",
          True,
          100e-6)
        self.pluto_source_0_0_0_0.set_metadata_keys('core:tx_freq', 'core:rx_freq', 'core:sample_start')
        self.plasma_spectrum_sink_0 = plasma.spectrum_sink(samp_rate, 128, center_freq, None, 0, False)
        self.plasma_spectrum_sink_0.set_metadata_keys('core:sample_rate', 'n_matrix_col', 'core:frequency', 'dynamic_range', 'radar:prf', 'radar:duration', 'detection_indices')
        self.plasma_spectrum_sink_0.set_dynamic_range(60)
        self.plasma_spectrum_sink_0.set_msg_queue_depth(1)
        self._plasma_spectrum_sink_0_win = sip.wrapinstance(self.plasma_spectrum_sink_0.pyqwidget(), Qt.QWidget)
        self.top_layout.addWidget(self._plasma_spectrum_sink_0_win)
        self.plasma_spectro_sink_0_3 = self.plasma_spectro_sink_0_3 = plasma.spectro_sink.make(samp_rate, 128, center_freq, False, None)
        self.plasma_spectro_sink_0_3.set_metadata_keys('samp_rate', 'fft_size', 'center_freq')
        self.plasma_spectro_sink_0_3.set_update_time(0.1)
        self.plasma_spectro_sink_0_3.set_msg_queue_depth(1)

        self.plasma_spectro_sink_0_3.set_db_path('/var/tmp/spectrogram.db')
        self.plasma_spectro_sink_0_3.set_db_enable(0)
        self.plasma_spectro_sink_0_3.set_device_id('01')

        self._plasma_spectro_sink_0_3_win = sip.wrapinstance(self.plasma_spectro_sink_0_3.qwidget(), Qt.QWidget)
        self.top_layout.addWidget(self._plasma_spectro_sink_0_3_win)
        self.plasma_signal_processing_0_3 = plasma.signal_processing(1024,samp_rate,1,1,0)


        ##################################################
        # Connections
        ##################################################
        self.msg_connect((self.plasma_signal_processing_0_3, 'out'), (self.plasma_spectro_sink_0_3, 'in'))
        self.msg_connect((self.plasma_signal_processing_0_3, 'out'), (self.plasma_spectrum_sink_0, 'in'))
        self.msg_connect((self.pluto_source_0_0_0_0, 'out'), (self.plasma_signal_processing_0_3, 'rx'))


    def closeEvent(self, event):
        self.settings = Qt.QSettings("GNU Radio", "TEST")
        self.settings.setValue("geometry", self.saveGeometry())
        self.stop()
        self.wait()

        event.accept()

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate

    def get_center_freq(self):
        return self.center_freq

    def set_center_freq(self, center_freq):
        self.center_freq = center_freq




def main(top_block_cls=TEST, options=None):

    qapp = Qt.QApplication(sys.argv)

    tb = top_block_cls()

    tb.start()

    tb.show()

    def sig_handler(sig=None, frame=None):
        tb.stop()
        tb.wait()

        Qt.QApplication.quit()

    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    timer = Qt.QTimer()
    timer.start(500)
    timer.timeout.connect(lambda: None)

    qapp.exec_()

if __name__ == '__main__':
    main()
