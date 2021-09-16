1. Files
This distribution contains the following files:
     a) README.txt: this file
     b) *.csv.gz: I/O trace files, described below
     c) MD5.txt: output of md5sum for csv.gz files

2. I/O trace files
There are 2706 I/O traces from 6 different block storage devices (LUNs 0,1,2,3,4, and 6).
More information about these is available from the SYSTOR '17 paper (see below).

Each trace file is named as <date>_<lun index>.csv.gz.

The date format consists of year(4 char.),month(2 char.),day(2 char.), and hour(2 char.).
The LUN index corresponds to each block storage device.
In the traces, the lun indexes are only 0,1,2,3,4, and 6.

Here is an example file name.
2016022219-LUN4.csv.gz
  - year : 2016
  - month : 02 (Feb.)
  - day : 22nd
  - hour : 7 PM
  - LUN index : 4

3. I/O trace file format
The files are gzipped csv (comma-separated text) files. The fields in
the csv are:

Timestamp,Response,IOType,LUN,Offset,Size

  - Timestamp is the time the I/O was issued.
    The timestamp is given as a Unix time (seconds since 1/1/1970) with a fractional part. 
    Although the fractional part is nine digits, it is accurate only to the microsecond level; 
    please  ignore the nanosecond part.  
    If you need to process the timestamps in their original local timezone, it is UTC+0900 (JST).
    For example:
     > head 2016022219-LUN4.csv.gz  ← (Mon, 22 Feb 2016 19:00:00 JST)
       1456135200.013118000 ← (Mon, 22 Feb 2016 10:00:00 GMT)       
  - Response is the time needed to complete the I/O.
  - IOType is "Read(R)", "Write(W)", or ""(blank).
    The blank indicates that there was no response message.
  - LUN is the LUN index (0,1,2,3,4, or 5).
  - Offset is the starting offset of the I/O in bytes from the start of
    the logical disk.
  - Size is the transfer size of the I/O request in bytes.

4. Additional information
When we cleaned up our traces, we found additional traces.
We unfortunately detected some packet loss (below 0.1%)
due to the way port-mirroring works.
Finally, for a small number of transactions we could not find a mapping
between OX_ID and RX_ID in a single binary dump file.
As a result, they became the unknown transactions mentioned above.

Here, we summarize the additional information:

  a) The additional measurements (Feb.16.2016 ~ Feb.21.2016)
     and extra hour (e.g. 7 AM) are included to this distribution.
  b) Due to the packet loss, some unknown transactions and
     irregular response times are included in the traces.
     Moreover, there was packet loss for a few seconds
     whenever the hour changed [e.g.) 9 AM -> 10 AM].
     Please ignore those transactions.
  c) In order to fix the unknown transactions,
     we modified them using OX_ID and RX_ID with a series of dump files.
     Through the modification, we recovered a few more
     transactions in the entire traces.
     We also confirmed the CDF curves, and
     there was no impact on the statistics.

5. Contact address
Please contact us via e-mail if you have any questions.
E-mail : systor_dataset_2017(at)ml.labs.fujitsu.com

6. Attribution.
Please cite the following publication as a reference in any published
work using these traces.
#Text
-----
Understanding storage traffic characteristics on enterprise virtual desktop
infrastructure
Chunghan Lee, Tatsuo Kumano, Tatsuma Matsuki, Hiroshi Endo, Naoto Fukumoto, and
Mariko Sugawara
Fujitsu Laboratories Ltd.
Proc. of the 10th ACM International Systems and Storage Conference (SYSTOR '17)
https://dl.acm.org/citation.cfm?id=3078479
-----

#Bibtex
-----
@inproceedings{Lee2017Understanding,
author = {Lee, Chunghan and Kumano, Tatsuo and Matsuki, Tatsuma and Endo, Hiroshi
and Fukumoto, Naoto and Sugawara, Mariko},
title = {Understanding Storage Traffic Characteristics on Enterprise Virtual Desktop
Infrastructure},
booktitle = {Proceedings of the 10th ACM International Systems and Storage
Conference},
series = {SYSTOR '17},
year = {2017},
isbn = {978-1-4503-5035-8},
location = {Haifa, Israel},
pages = {13:1--13:11},
articleno = {13},
numpages = {11},
publisher = {ACM},
address = {New York, NY, USA},
keywords = {measurement, performance analysis, virtual desktop infrastructure (VDI)},
}
-----
