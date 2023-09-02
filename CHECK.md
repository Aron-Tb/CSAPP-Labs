## Data-Lab:
./btest

## Boom-Lab:
./bomb < ans.txt

## Target-Lab:
./ctarget -i ./solutions/output1.txt -q
./ctarget -i ./solutions/output2.txt -q
./ctarget -i ./solutions/output3.txt -q
./rtarget -i ./solutions/output4.txt -q

## Cache-Lab:
./driver.py

## Shell-Lab:
make testxx 和 make rtestxx 对比输出结果

## Malloc-Lab:
./mdriver -t ./traces -V

## Proxy-Lab:
./driver.sh