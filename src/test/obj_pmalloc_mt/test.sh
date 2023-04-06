
#!/bin/bash

rm -rf /mnt/pmem/*
for TGH in {1..100}
do
echo $TGH
LD_LIBRARY_PATH=/home/tgromadz/repos/pmdk/src/debug; ./obj_pmalloc_mt 2 8000 100 /mnt/pmem/obj_pmalloc_mt
status=$?
if [ $status -gt 0 ]
then
	exit 1
fi
done
