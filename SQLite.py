#!/usr/bin/env python3
import os, sqlite3, math, argparse, time
import numpy as np
import matplotlib.pyplot as plt

DB = os.getenv("PLASMA_DB_PATH", "/var/tmp/spectrogram.db")

def fetch_latest_by_center(con, ts_us, limit=1000, bucket_hz=1e5):
    rows = con.execute("""
      SELECT ts_us, center_hz, bin0_hz, df_hz, power_db
      FROM spectrogram_frames
      WHERE ts_us <= ?
      ORDER BY ts_us DESC
      LIMIT ?
    """, (int(ts_us), limit)).fetchall()
    groups = {}
    for ts, cf, f0, df, blob in rows:
        b = int(round(cf / bucket_hz))
        if b not in groups:
            groups[b] = (ts, cf, f0, df, blob)
    devs = [(ts, cf, f0, df, np.frombuffer(blob, dtype="<f4")) for (ts, cf, f0, df, blob) in groups.values()]
    devs.sort(key=lambda x: x[1])
    return devs

def build_grid(devs, cap_bins=65536):
    fmins = [f0 for (_, _, f0, df, vals) in devs]
    fmaxs = [f0 + (vals.size - 1) * df for (_, _, f0, df, vals) in devs]
    dfs   = [df for (_, _, _, df, _) in devs]
    Fmin, Fmax = min(fmins), max(fmaxs)
    df_min = min(dfs)
    nb = int(math.floor((Fmax - Fmin)/df_min)) + 1
    if nb > cap_bins:
        df_min *= math.ceil(nb / cap_bins)
        nb = int(math.floor((Fmax - Fmin)/df_min)) + 1
    freq = Fmin + np.arange(nb) * df_min
    return freq, Fmin, Fmax, df_min

def to_grid(freq, f0, df, vals):
    f_local = f0 + np.arange(vals.size)*df
    return np.interp(freq, f_local, vals, left=np.nan, right=np.nan)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default=DB)
    ap.add_argument("--win-sec", type=float, default=30.0, help="최근 몇 초를 그릴지")
    ap.add_argument("--period",  type=float, default=0.1, help="줄 간격(초)")
    ap.add_argument("--bucket-hz", type=float, default=1e5, help="장치 그룹핑용 센터주파수 버킷 폭")
    ap.add_argument("--cap-bins", type=int, default=65536, help="통합 그리드 최대 bin 수")
    ap.add_argument("--save", default="", help="PNG로 저장 경로(비우면 창 표시)")
    args = ap.parse_args()

    con = sqlite3.connect(args.db, timeout=5.0)

    (ts_max,) = con.execute("SELECT MAX(ts_us) FROM spectrogram_frames").fetchone()
    if ts_max is None:
        print("DB에 프레임이 없습니다."); return
    t_end   = ts_max / 1e6
    t_start = t_end - args.win_sec

    ts_list = np.arange(int(t_start*1e6), int(t_end*1e6)+1, int(args.period*1e6), dtype=np.int64)
    if ts_list.size == 0:
        print("시간 윈도우가 너무 짧습니다."); return

    devs0 = fetch_latest_by_center(con, ts_list[0], bucket_hz=args.bucket_hz)
    if not devs0:
        print("해당 구간에 프레임이 없습니다."); return
    freq, Fmin, Fmax, df_min = build_grid(devs0, cap_bins=args.cap_bins)

    wf = np.full((ts_list.size, freq.size), np.nan, dtype=np.float32)

    for i, t_us in enumerate(ts_list):
        devs = fetch_latest_by_center(con, t_us, bucket_hz=args.bucket_hz)
        if not devs:
            continue
        fmins = [d[2] for d in devs]
        fmaxs = [d[2] + (d[4].size-1)*d[3] for d in devs]
        need_expand = (min(fmins) < freq[0]) or (max(fmaxs) > freq[-1])
        if need_expand:
            freq_new, Fmin, Fmax, df_min = build_grid(devs, cap_bins=args.cap_bins)
            wf_new = np.full((wf.shape[0], freq_new.size), np.nan, dtype=np.float32)
            for r in range(wf.shape[0]):
                row = wf[r]
                m = ~np.isnan(row)
                if m.any():
                    wf_new[r] = np.interp(freq_new, freq[m], row[m], left=np.nan, right=np.nan)
            wf = wf_new
            freq = freq_new

        line = np.full(freq.size, np.nan, dtype=np.float32)
        for (_, _, f0, df, vals) in devs:
            y = to_grid(freq, f0, df, vals)
            m = ~np.isnan(line); n = ~np.isnan(y)
            both = m & n    
            line[both] = np.maximum(line[both], y[both])
            line[~m & n] = y[~m & n]
        wf[i] = line

    fig, ax = plt.subplots()
    im = ax.imshow(
        wf,
        aspect='auto',
        origin='upper',
        extent=[freq[0], freq[-1], 0, wf.shape[0]],
        interpolation='nearest',
        # vmin=-100,  
        # vmax=-20    
    )

    plt.colorbar(im, ax=ax, label="Power (dB)")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel(f"frames @ {args.period:.3f}s")
    ax.set_title(f"Stitched Waterfall (last {args.win_sec:.0f}s)")
    plt.tight_layout()

    if args.save:
        plt.savefig(args.save, dpi=150)
        print("saved:", args.save)
    else:
        plt.show()

if __name__ == "__main__":
    main()
