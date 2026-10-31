import sys, re

out_path, carriers_path, origins_path = sys.argv[1:4]
in_files = sys.argv[4:]

CARRIER_RE = re.compile(r'^[A-Z0-9]{2}$')
AIRPORT_RE = re.compile(r'^[A-Z]{3}$')

stats = dict(total=0, headers=0, bad_columns=0, bad_month=0,
             bad_carrier_or_airport=0, bad_cancelled_flag=0,
             na_delay=0, bad_number=0, kept=0, kept_cancelled=0)
carriers, origins = set(), set()

with open(out_path, 'w') as out:
    for path in in_files:
        with open(path) as f:
            for line in f:
                p = line.rstrip('\r\n').split(',')
                if p[0] == 'Year':
                    stats['headers'] += 1
                    continue
                stats['total'] += 1
                if len(p) != 29:
                    stats['bad_columns'] += 1; continue
                year, month, day, dow = p[0], p[1], p[2], p[3]
                carrier, flightnum = p[8], p[9]
                arr, dep = p[14], p[15]
                origin, dest, dist, cancelled = p[16], p[17], p[18], p[21]

                if not (month.isdigit() and 1 <= int(month) <= 12):
                    stats['bad_month'] += 1; continue
                if not (CARRIER_RE.match(carrier) and AIRPORT_RE.match(origin)
                        and AIRPORT_RE.match(dest)):
                    stats['bad_carrier_or_airport'] += 1; continue
                if cancelled not in ('0', '1'):
                    stats['bad_cancelled_flag'] += 1; continue

                if cancelled == '1':
                    dep, arr = '0', '0'          # cancelled flights have no delay
                    if dist == 'NA': dist = '0'
                elif dep == 'NA' or arr == 'NA':
                    stats['na_delay'] += 1; continue   # includes diverted flights

                try:
                    for x in (year, day, dow, flightnum, dist, dep, arr):
                        int(x)
                except ValueError:
                    stats['bad_number'] += 1; continue

                out.write(','.join([year, month, day, dow, carrier, flightnum,
                                    origin, dest, dist, cancelled, dep, arr]) + '\n')
                stats['kept'] += 1
                if cancelled == '1': stats['kept_cancelled'] += 1
                carriers.add(carrier); origins.add(origin)

open(carriers_path, 'w').write('\n'.join(sorted(carriers)) + '\n')
open(origins_path, 'w').write('\n'.join(sorted(origins)) + '\n')
for k, v in stats.items():
    print(f'{k:25s} {v}')