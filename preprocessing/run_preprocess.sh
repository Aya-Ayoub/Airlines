set -e
cd "$(dirname "$0")/.."

echo "=== BEFORE ==="
du -ch data/raw/*.csv | tail -1
echo "raw lines (including 1 header per file): $(cat data/raw/*.csv | wc -l)"

echo "=== CLEANING ==="
python3 preprocessing/preprocess.py data/unsorted.csv data/carriers.txt data/origins.txt data/raw/*.csv

echo "=== REMOVING DUPLICATES ==="
LC_ALL=C sort -u -T data data/unsorted.csv -o data/airline_clean.csv
rm data/unsorted.csv

echo "=== AFTER ==="
echo "records after dedupe: $(wc -l < data/airline_clean.csv)"
ls -lh data/airline_clean.csv
echo "carriers: $(wc -l < data/carriers.txt)   airports: $(wc -l < data/origins.txt)"