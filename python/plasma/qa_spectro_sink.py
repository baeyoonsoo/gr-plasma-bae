#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2022 gr-plasma author.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

import os
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")  # 헤드리스 환경 대비

from gnuradio import gr, gr_unittest, blocks
import pmt

try:
    from gnuradio.plasma import spectro_sink
except ImportError:
    import sys
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    from gnuradio.plasma import spectro_sink


class qa_spectro_sink(gr_unittest.TestCase):

    def setUp(self):
        self.tb = gr.top_block()

    def tearDown(self):
        if self.tb is not None:
            self.tb.stop()
            self.tb.wait()
        self.tb = None

    def _make_pdu(self, fft_size=1024, samp_rate=1.0e6, center_freq=100e6):
        # 샘플 데이터 (float32 벡터)
        data = [0.0] * fft_size
        vec = pmt.init_f32vector(fft_size, data)

        # 메타데이터 (fft_size, samp_rate, center_freq)
        meta = pmt.make_dict()
        meta = pmt.dict_add(meta, pmt.intern("fft_size"),    pmt.from_long(fft_size))
        meta = pmt.dict_add(meta, pmt.intern("samp_rate"),   pmt.from_double(samp_rate))
        meta = pmt.dict_add(meta, pmt.intern("center_freq"), pmt.from_double(center_freq))

        # PDU = (meta . vec)
        return pmt.cons(meta, vec)

    def test_instance(self):
        # 유효 인자로 인스턴스가 생성되는지 확인
        samp_rate    = 1.0e6
        fft_size     = 1024
        ncol         = fft_size
        center_freq  = 100e6
        parent_qw    = None  # QWidget* parent

        inst = spectro_sink(samp_rate, fft_size, ncol, center_freq, parent_qw)
        # 옵션: 업데이트 주기/큐 길이 살짝 조정해 테스트 안정화
        try:
            inst.set_update_time(0.01)
            inst.set_msg_queue_depth(10)
        except AttributeError:
            # 바인딩에 없을 수도 있음 — 통과만
            pass

        self.assertIsNotNone(inst)

    def test_pdu_ingest_once(self):
        # message_strobe로 PDU 한 번 발사 → GUI 이벤트 큐로 안전히 전달되는지만 본다
        samp_rate    = 1.0e6
        fft_size     = 256
        ncol         = fft_size
        center_freq  = 915e6
        parent_qw    = None

        sink = spectro_sink(samp_rate, fft_size, ncol, center_freq, parent_qw)

        # 테스트 실행 속도/부하를 낮추기 위한 설정(있으면)
        try:
            sink.set_update_time(0.01)
            sink.set_msg_queue_depth(50)
        except AttributeError:
            pass

        pdu = self._make_pdu(fft_size=fft_size, samp_rate=samp_rate, center_freq=center_freq)
        strobe = blocks.message_strobe(pdu, 10)  # 10 ms 간격 (run 시간을 짧게 잡으니 1~2회만 발사될 것)

        # 포트명: message_strobe의 출력은 "strobe", spectro_sink의 입력은 "in"
        self.tb.msg_connect(strobe, "strobe", sink, "in")

        # 짧게 돌려서 크래시 없이 메시지 소비만 확인
        self.tb.start()
        self.tb.wait_until_finished(0.2)
        self.tb.stop()
        self.tb.wait()

        # 여기서는 크래시/예외 없이 돌아가면 성공
        self.assertTrue(True)


if __name__ == '__main__':
    gr_unittest.run(qa_spectro_sink)
