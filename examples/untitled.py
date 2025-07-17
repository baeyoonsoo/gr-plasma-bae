#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# SPDX-License-Identifier: GPL-3.0
#
# GNU Radio Python Flow Graph
# Title: Not titled yet
# GNU Radio version: 3.10.1.1

from packaging.version import Version as StrictVersion

if __name__ == '__main__':
    import ctypes
    import sys
    if sys.platform.startswith('linux'):
        try:
            x11 = ctypes.cdll.LoadLibrary('libX11.so')
            x11.XInitThreads()
        except:
            print("Warning: failed to XInitThreads()")

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



from gnuradio import qtgui

class untitled(gr.top_block, Qt.QWidget):

    def __init__(self):
        gr.top_block.__init__(self, "Not titled yet", catch_exceptions=True)
        Qt.QWidget.__init__(self)
        self.setWindowTitle("Not titled yet")
        qtgui.util.check_set_qss()
        try:
            self.setWindowIcon(Qt.QIcon.fromTheme('gnuradio-grc'))
        except:
            pass
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

        self.settings = Qt.QSettings("GNU Radio", "untitled")

        try:
            if StrictVersion(Qt.qVersion()) < StrictVersion("5.0.0"):
                self.restoreGeometry(self.settings.value("geometry").toByteArray())
            else:
                self.restoreGeometry(self.settings.value("geometry"))
        except:
            pass

        ##################################################
        # Variables
        ##################################################
        self.samp_rate = samp_rate = 20e6
        self.n_pulse_cpi = n_pulse_cpi = 128

        ##################################################
        # Blocks
        ##################################################
        self.pluto_source_bae_0 = plasma.pluto_radar_bae(
          'ip:192.168.2.1',
          2400000000,
          20000000,
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
          "",
          True)
        self.pluto_source_bae_0.set_metadata_keys('core:tx_freq', 'core:rx_freq', 'core:sample_start')
        self.plasma_range_doppler_sink_0 = plasma.range_doppler_sink(samp_rate, n_pulse_cpi, 2.3e9)
        self.plasma_range_doppler_sink_0.set_metadata_keys('core:sample_rate', 'n_matrix_col', 'core:tx_freq', 'dynamic_range', 'radar:prf', 'radar:duration', 'detection_indices')
        self.plasma_range_doppler_sink_0.set_dynamic_range(60)
        self.plasma_range_doppler_sink_0.set_msg_queue_depth(10)
        self._plasma_range_doppler_sink_0_win = sip.wrapinstance(self.plasma_range_doppler_sink_0.pyqwidget(), Qt.QWidget)
        self.top_layout.addWidget(self._plasma_range_doppler_sink_0_win)
        self.plasma_pulse_to_cpi_0 = plasma.pulse_to_cpi(n_pulse_cpi)
        self.plasma_pulse_to_cpi_0.init_meta_dict('radar:n_pulse_cpi')
        self.plasma_pulse_doppler_0 = plasma.pulse_doppler(n_pulse_cpi, n_pulse_cpi)
        self.plasma_pulse_doppler_0.set_msg_queue_depth(100)
        self.plasma_pulse_doppler_0.set_backend(plasma.Device.CUDA)
        self.plasma_pulse_doppler_0.init_meta_dict('doppler_fft_size')
        self.plasma_lfm_source_0 = plasma.lfm_source(samp_rate/2, -samp_rate/4, 10e-6, samp_rate, 0)
        self.plasma_lfm_source_0.init_meta_dict('radar:bandwidth', 'radar:start_freq', 'radar:duration', 'core:sample_rate', 'core:label', 'radar:prf')
        self.plasma_cw_to_pulsed_0 = plasma.cw_to_pulsed(10e3, samp_rate)
        self.plasma_cw_to_pulsed_0.init_meta_dict('core:sample_rate', 'radar:prf')


        ##################################################
        # Connections
        ##################################################
        self.msg_connect((self.plasma_cw_to_pulsed_0, 'out'), (self.pluto_source_bae_0, 'in'))
        self.msg_connect((self.plasma_lfm_source_0, 'out'), (self.plasma_cw_to_pulsed_0, 'in'))
        self.msg_connect((self.plasma_lfm_source_0, 'out'), (self.plasma_pulse_doppler_0, 'tx'))
        self.msg_connect((self.plasma_pulse_doppler_0, 'out'), (self.plasma_range_doppler_sink_0, 'in'))
        self.msg_connect((self.plasma_pulse_to_cpi_0, 'out'), (self.plasma_pulse_doppler_0, 'rx'))
        self.msg_connect((self.pluto_source_bae_0, 'out'), (self.plasma_pulse_to_cpi_0, 'in'))


    def closeEvent(self, event):
        self.settings = Qt.QSettings("GNU Radio", "untitled")
        self.settings.setValue("geometry", self.saveGeometry())
        self.stop()
        self.wait()

        event.accept()

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate

    def get_n_pulse_cpi(self):
        return self.n_pulse_cpi

    def set_n_pulse_cpi(self, n_pulse_cpi):
        self.n_pulse_cpi = n_pulse_cpi




def main(top_block_cls=untitled, options=None):

    if StrictVersion("4.5.0") <= StrictVersion(Qt.qVersion()) < StrictVersion("5.0.0"):
        style = gr.prefs().get_string('qtgui', 'style', 'raster')
        Qt.QApplication.setGraphicsSystem(style)
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
