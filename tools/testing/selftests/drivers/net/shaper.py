#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0

from lib.py import ksft_run, ksft_exit, ksft_eq, KsftSkipEx
from lib.py import ShaperFamily
from lib.py import NetDrvEnv
from lib.py import NlError
from lib.py import cmd
import glob

def get_shapers(cfg, nl_shaper) -> None:
    shapers = nl_shaper.get({'ifindex': cfg.ifindex}, dump=True)
    ksft_eq(len(shapers), 0)


def set_shapers(cfg, nl_shaper) -> None:
    nl_shaper.set({'ifindex': cfg.ifindex,
                   'shapers': [{ 'handle': { 'scope': 'queue', 'minor': 1 }, 'bw_min': 10000 },
                               { 'handle': { 'scope': 'queue', 'minor': 2 }, 'bw_min': 20000 }]})

    raised = False
    try:
        shaper_q0 = nl_shaper.get({'ifindex': cfg.ifindex, 'handle': { 'scope': 'queue', 'minor': 0}})
    except (NlError):
        raised = True
    ksft_eq(raised, True)

    shaper_q1 = nl_shaper.get({'ifindex': cfg.ifindex, 'handle': { 'scope': 'queue', 'minor': 1 }})
    ksft_eq(shaper_q1, { 'handle': { 'scope': 'queue', 'major': 0, 'minor': 1 },
                          'metric': 'pps',
                          'bw_min': 10000,
                          'bw_max': 0,
                          'burst': 0,
                          'priority': 0,
                          'weight': 0 })

    shapers = nl_shaper.get({'ifindex': cfg.ifindex}, dump=True)
    ksft_eq(shapers, [{'handle': { 'scope': 'queue', 'major': 0, 'minor': 1 },
                       'metric': 'pps',
                       'bw_min': 10000,
                       'bw_max': 0,
                       'burst': 0,
                       'priority': 0,
                       'weight': 0 },
                      {'handle': { 'scope': 'queue', 'major': 0, 'minor': 2 },
                       'metric': 'pps',
                       'bw_min': 20000,
                       'bw_max': 0,
                       'burst': 0,
                       'priority': 0,
                       'weight': 0 }])


def del_shapers(cfg, nl_shaper) -> None:
    raised = False
    try:
        nl_shaper.delete({'ifindex': cfg.ifindex, 'handles': [ { 'scope': 'queue', 'minor': 0}]})
    except (NlError):
        raised = True
    ksft_eq(raised, False)

    nl_shaper.delete({'ifindex': cfg.ifindex,
                      'handles': [{ 'scope': 'queue', 'minor': 2},
                                  { 'scope': 'queue', 'minor': 1}]})
    shapers = nl_shaper.get({'ifindex': cfg.ifindex}, dump=True)
    ksft_eq(len(shapers), 0)

def main() -> None:
    with NetDrvEnv(__file__, queue_count=3) as cfg:
        ksft_run([get_shapers, set_shapers, del_shapers], args=(cfg, ShaperFamily()))
    ksft_exit()


if __name__ == "__main__":
    main()
