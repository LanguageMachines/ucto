#! /bin/sh

pwd

ucto -c $srcdir/../tests/tst.cfg $srcdir/../tests/tst.txt tst.out
diff tst.out $srcdir/../tests/tst.ok
