# Win-df
A "df-like" program for the Windows command line.  Here's the program's help output:

```
DF - Disk Free - v5.1
     Copyright (C) 1996-2022, by Steve Valliere
     All Rights Reserved

     usage:  df [/A] [/c] [/j] [/m] [/n] [/p] [/r] [/s]
             /A  check *ALL* drives
             /c  check CDROM drives
             /j  check mounted drives (Junctions)
             /m  check RAM (memory) drives
             /n  check network drives
             /p  check MOUNTED drives (Admin shell REQUIRED)
             /r  check removable drives
             /s  check SUBST drives

             /?  Show this help info
```
It is possible that I should remove the copyright in order to use the GPLv3, hopefully someone will let me know what is correct.  There is a minor issue with MOUNTED drives:  For some reason, at least one of the functions I'm using only works when the program is run under an Administrator account.

Here's an example of output:
```
C:\Users\myname>df /A

    [----------- b y t e s -----------]        true
drv     total        used        free    usage path
C:    459.737 G   295.682 G   164.054 G   64%  C:\
D:      4.000 T     2.141 T     1.859 T   53%  D:\
E:    500.090 G   282.055 G   218.034 G   56%  E:\
F:    500.090 G   289.681 G   210.408 G   57%  F:\
H:      3.000 T     2.072 T   927.477 G   69%  H:\
J:      3.000 T     1.995 T     1.004 T   66%  J:\
K:  The device is not ready.
N:      6.001 T   152.850 G     5.848 T    2%  \\192.168.1.1\share0\
O:      6.001 T     4.211 T     1.789 T   70%  \\192.168.1.1\share1\
S:      6.001 T     5.977 T    23.671 G   99%  \\192.168.1.1\share2\
T:      4.000 T     3.838 T   162.680 G   95%  \\192.168.1.1\share3\
U:      4.000 T     3.823 T   177.220 G   95%  \\192.168.1.1\share4\
V:      6.001 T     4.465 T     1.536 T   74%  \\192.168.1.1\share5\
W:      4.000 T     2.716 T     1.284 T   67%  \\192.168.1.1\share6\
X:      4.000 T     2.895 T     1.104 T   72%  \\192.168.1.1\share7\
Y:      4.000 T     2.777 T     1.223 T   69%  \\192.168.1.1\share8\
Z:      4.000 T     2.853 T     1.147 T   71%  \\192.168.1.1\share9\
<*>     3.000 T     1.054 T     1.945 T   35%  D:\mnt\three\
<*>   640.132 G   471.548 G   168.583 G   73%  D:\mnt\sixforty\

    no removable drives detected
    no CDROM drives detected
    no RAM drives detected
    no SUBST drives detected
```
The `drv` column shows the drive letter, or `<*>` for MOUNTED drives, while the `true path` column shows the actual root path of the drive, or the path to the network share mounted _as_ the drive.  Hopefully, the numeric columns are self-evident.  (smile).

If you like this, feel free to use it.  If you can help me figure out how to avoid the requirement to be an Administrator to learn about MOUNTED drives, please, PLEASE help!
