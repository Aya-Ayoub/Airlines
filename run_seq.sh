for i in 1 2 3; do
  echo "--- run $i"
  ./build/seq $1 data/carriers.txt data/origins.txt results/tmp.csv | grep TIME
done