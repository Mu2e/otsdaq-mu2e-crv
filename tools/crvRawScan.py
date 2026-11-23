#!/usr/bin/env python3
"""Scan CRV raw data (art files) for readout faults, one line of facts per ROC.

For every event and every DTC link with a ROC it checks:
  - the DTC link status (0x81 = the ROC did not answer in time),
  - that the ROC sent data and the event window tag (EWT) in the ROC header
    is the one of the event,
  - the ROC status word (values other than the usual one are listed),
  - the ROC header counter: it must advance by one per event; every step
    is a request the ROC did not answer.

It runs `mu2e` with tools/fcl/CrvRawDtcPrint.fcl, so the offline environment
must be set up (on mu2e-dl-01: `cd ~/ots_ops && source setup_ots.sh`).

Examples
  crvRawScan.py 125846 --list                 # files of the run
  crvRawScan.py 125846 --event 56355077       # the file that holds this event
  crvRawScan.py 125846 --subrun 466           # one file
  crvRawScan.py 125846 --all                  # every file (about 2 min per file)
  crvRawScan.py --file a.art b.art
  crvRawScan.py 125846 --event 56355077 --show 56355070 56355080
"""
import argparse
import glob
import os
import re
import subprocess
import sys
from collections import Counter

EVENTS_PER_FILE = 600000  # data logger setting; only used to find a file by event number

# Only these pieces of the (very large) printout are kept.
GREP = (r'=== run: [0-9]* subRun: [0-9]* event: [0-9]*'
        r'\|"link[0-5]_status": [0-9]*'
        r'\|"link[0-5]_drp_rx_latency": [0-9]*'
        r'\|"byteCount": 0x[0-9a-f]*'
        r'\|"linkID": [0-9]'
        r'\|"event_tag": [0-9]*'
        r'\|Packet \[0\]: "DataPacket": {"data": \[[0-9a-fx,]*'
        r'\|mismatch of ROC [A-Za-z ]*')

RE_EVENT = re.compile(r'=== run: (\d+) subRun: (\d+) event: (\d+)')
RE_STAT = re.compile(r'"link(\d)_status": (\d+)')
RE_LAT = re.compile(r'"link(\d)_drp_rx_latency": (\d+)')


class Event:
    def __init__(self, number, subrun):
        self.number = number
        self.subrun = subrun
        self.status = {}   # link -> DTC link status
        self.latency = {}  # link -> DTC latency
        self.blocks = {}   # link -> [byteCount, tag, first packet words or None]
        self.decode_error = False
        self._cur = None

    def feed(self, tok):
        m = RE_STAT.match(tok)
        if m:
            self.status[int(m.group(1))] = int(m.group(2))
            return
        m = RE_LAT.match(tok)
        if m:
            self.latency[int(m.group(1))] = int(m.group(2))
            return
        if tok.startswith('"byteCount"'):
            self._cur = [int(tok.split()[-1], 16), None, None]
        elif tok.startswith('"linkID"') and self._cur is not None:
            self.blocks[int(tok.split()[-1])] = self._cur
        elif tok.startswith('"event_tag"') and self._cur is not None:
            self._cur[1] = int(tok.split()[-1])
        elif tok.startswith('Packet') and self._cur is not None:
            words = tok[tok.index('[', tok.index('data')) + 1:]
            self._cur[2] = [int(x, 16) for x in words.split(',') if x]
        elif tok.startswith('mismatch'):
            self.decode_error = True

    def line(self, links):
        out = ['%d' % self.number]
        for k in links:
            b = self.blocks.get(k)
            s = 'link%d: dtc status 0x%x latency %s' % (k, self.status.get(k, 0),
                                                        self.latency.get(k, '-'))
            if b is None:
                s += ' no block'
            else:
                s += ' bytes %d' % b[0]
                if b[2]:
                    s += ' roc header ' + ' '.join('%04x' % w for w in b[2][:8])
            out.append(s)
        return ' | '.join(out)


class LinkStats:
    def __init__(self, link):
        self.link = link
        self.kinds = Counter()
        self.roc_status = Counter()
        self.first = {}            # kind -> first event
        self.examples = []         # (event, kind)
        self.steps = []            # (event, offset before, offset after)
        self.offset = None
        self.odd_status = []       # (event, status)
        self.latency = []
        self.size = []
        self.last_ok = None
        self.timeouts_since_ok = 0
        self.break_event = None    # first event of the timeouts that last to the end

    def add(self, ev):
        k = self.link
        st = ev.status.get(k, 0)
        b = ev.blocks.get(k)
        kind = 'ok'
        if st != 0:
            kind = 'dtc status 0x%x' % st
        elif b is None:
            kind = 'no block'
        elif not b[2] or len(b[2]) < 8:
            kind = 'block header only'
        else:
            bc, tag, p = b
            ewt = (p[6] << 16) | p[7]
            status = (p[4] << 16) | p[5]
            off = (p[1] - ev.number) & 0xffff
            if self.offset is not None and off != self.offset:
                self.steps.append((ev.number, self.offset, off))
            self.offset = off
            if tag is not None and (tag & 0xffffffff) != (ev.number & 0xffffffff):
                kind = 'block tag != event'
            elif ewt != (ev.number & 0xffffffff):
                kind = 'ROC EWT != event'
            elif p[0] <= 8:
                kind = 'ok, no hits'
            self.roc_status[status] += 1
            if self.roc_status[status] <= 20:
                self.odd_status.append((ev.number, status, bc))
            self.latency.append(ev.latency.get(k, 0))
            self.size.append(bc)
        self.kinds[kind] += 1
        if kind.startswith('ok'):
            self.last_ok = ev.number
            self.timeouts_since_ok = 0
            self.break_event = None
        else:
            self.first.setdefault(kind, ev.number)
            if len(self.examples) < 20:
                self.examples.append((ev.number, kind))
            if self.timeouts_since_ok == 0:
                self.break_event = ev.number
            self.timeouts_since_ok += 1

    def report(self):
        total = sum(self.kinds.values())
        print('link %d: %d events' % (self.link, total))
        for kind, n in self.kinds.most_common():
            first = '' if kind.startswith('ok') else ', first at event %d' % self.first[kind]
            print('    %-22s %9d%s' % (kind, n, first))
        if self.break_event is not None and self.timeouts_since_ok > 1:
            print('    BREAK: no good event from %d (EWT 0x%x) to the end of the scan (%d events);'
                  ' last good event %s' % (self.break_event, self.break_event,
                                           self.timeouts_since_ok, self.last_ok))
        if self.roc_status:
            usual = self.roc_status.most_common(1)[0][0]
            print('    ROC status word: usual 0x%08x' % usual)
            odd = [(e, s, bc) for e, s, bc in self.odd_status if s != usual]
            for e, s, bc in odd[:20]:
                print('        event %d: status 0x%08x, %d bytes' % (e, s, bc))
            n_odd = sum(self.roc_status.values()) - self.roc_status[usual]
            if n_odd > len(odd[:20]):
                print('        ... %d events with another status in total' % n_odd)
        if self.steps:
            print('    ROC header counter stepped %d time(s) (requests without answer):'
                  % len(self.steps))
            for e, a, b in self.steps[:20]:
                print('        at event %d: %d request(s) lost' % (e, (a - b) & 0xffff))
            if len(self.steps) > 20:
                print('        ... %d more' % (len(self.steps) - 20))
        elif self.offset is not None:
            print('    ROC header counter in step for all good events')
        if self.size:
            a = sorted(self.latency)
            b = sorted(self.size)
            n = len(a)
            print('    DTC latency median %d, 99.9%% %d, max %d; bytes median %d, 99.9%% %d, max %d'
                  % (a[n // 2], a[int(n * .999)], a[-1], b[n // 2], b[int(n * .999)], b[-1]))
        bad = [x for x in self.examples]
        if bad:
            print('    first faults: ' + ', '.join('%d (%s)' % x for x in bad[:8]))


def run_files(run, directory):
    files = glob.glob(os.path.join(directory, '*.%s_*.art' % run))

    def subrun(f):
        m = re.search(r'\.%s_(\d+)' % run, os.path.basename(f))
        return int(m.group(1)) if m else -1
    return sorted(((subrun(f), f) for f in files if subrun(f) >= 0))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('run', nargs='?', help='run number')
    ap.add_argument('--dir', default='/data/DAQ/staged',
                    help='directory with the raw files (default %(default)s; '
                         'a few sample subruns are in /scratch/look_area/kpp_crv)')
    ap.add_argument('--list', action='store_true', help='list the files of the run and stop')
    ap.add_argument('--subrun', type=int, nargs='+', help='scan the files of these subruns')
    ap.add_argument('--event', type=int, help='scan the file that holds this event')
    ap.add_argument('--all', action='store_true', help='scan all files of the run')
    ap.add_argument('--file', nargs='+', help='scan these files')
    ap.add_argument('--links', default='0,3',
                    help='DTC links with a ROC (default %(default)s)')
    ap.add_argument('--show', type=int, nargs=2, metavar=('FIRST', 'LAST'),
                    help='also print one line per event in this event range')
    ap.add_argument('--save', help='write one line per event to this file')
    ap.add_argument('--fcl', default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                  'fcl', 'CrvRawDtcPrint.fcl'))
    args = ap.parse_args()
    links = [int(x) for x in args.links.split(',')]

    if args.file:
        files = args.file
    else:
        if not args.run:
            ap.error('give a run number or --file')
        found = run_files(args.run, args.dir)
        if not found:
            sys.exit('no files of run %s in %s' % (args.run, args.dir))
        if args.list:
            for i, (s, f) in enumerate(found):
                print('subrun %6d  about events %9d - %9d  %s'
                      % (s, i * EVENTS_PER_FILE, (i + 1) * EVENTS_PER_FILE - 1, f))
            return
        if args.all:
            files = [f for _, f in found]
        elif args.subrun:
            files = [f for s, f in found if s in args.subrun]
        elif args.event is not None:
            i = args.event // EVENTS_PER_FILE
            if i >= len(found):
                sys.exit('event %d would be in file %d, the run has %d files in %s (the data '
                         'logger can be behind)' % (args.event, i + 1, len(found), args.dir))
            files = [found[i][1]]
        else:
            ap.error('give --list, --all, --subrun, --event or --file')
        if not files:
            sys.exit('no file selected')

    cmd = 'mu2e -c %s %s 2>&1 | grep -o \'%s\'' % (
        args.fcl, ' '.join('-s ' + f for f in files), GREP)
    print('scanning %d file(s): %s' % (len(files), ' '.join(os.path.basename(f) for f in files)))
    proc = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, universal_newlines=True)

    stats = dict((k, LinkStats(k)) for k in links)
    save = open(args.save, 'w') if args.save else None
    n = 0
    no_subevent = 0
    decode_errors = 0
    first_ev = last_ev = None
    shown = []

    def finish(ev):
        nonlocal n, no_subevent, decode_errors, first_ev, last_ev
        n += 1
        if first_ev is None:
            first_ev = ev.number
        last_ev = ev.number
        if ev.decode_error:
            decode_errors += 1
        if not ev.status and not ev.blocks:
            no_subevent += 1
        else:
            for k in links:
                stats[k].add(ev)
        if save:
            save.write(ev.line(links) + '\n')
        if args.show and args.show[0] <= ev.number <= args.show[1]:
            shown.append(ev.line(links))

    ev = None
    for tok in proc.stdout:
        tok = tok.rstrip('\n')
        m = RE_EVENT.match(tok)
        if m:
            if ev is not None:
                finish(ev)
            ev = Event(int(m.group(3)), int(m.group(2)))
        elif ev is not None:
            ev.feed(tok)
    if ev is not None:
        finish(ev)
    proc.wait()
    if save:
        save.close()

    if n == 0:
        sys.exit('no events read: is the mu2e environment set up and the file readable?')
    print('%d events, %s to %s; without DTC subevent %d; decode errors %d'
          % (n, first_ev, last_ev, no_subevent, decode_errors))
    for k in links:
        stats[k].report()
    if shown:
        print()
        for line in shown:
            print(line)


if __name__ == '__main__':
    main()
