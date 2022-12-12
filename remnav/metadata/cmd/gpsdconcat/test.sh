#!/bin/bash
DRYRUN="" # "--dryrun"
for sessionid in 000 001 11_28_2022
do
	./gpsdconcat ${DRYRUN} -machine_id 4cb50f9c2aed73a74990c66eab314d31ea94526241c656536005a95fcacae79e -raw_root local -archive_root ./Captures ${sessionid}
done
