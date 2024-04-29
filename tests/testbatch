#/bin/sh

\rm -rf batch_out*
\rm -rf uit*

echo "Start"
$exe -L nld -B folia1.xml test.nl.txt >& uit1
cat < uit1

echo "Step 1"
$exe -L nld -B folia1.xml test.nl.txt folia7.xml -O batch_out1
$folialint --nooutput batch_out1/folia1.ucto.xml 2>&1
cat < batch_out1/test.nl.ucto.txt
$folialint --nooutput batch_out1/folia7.ucto.xml 2>&1

echo "Step 2"
$exe -L fra -B -X test.fr.txt multilang.txt -O batch_out2
$folialint --nooutput batch_out2/test.fr.txt.ucto.xml 2>&1
$folialint --nooutput batch_out2/multilang.txt.ucto.xml 2>&1

echo "Step 3"
$exe -L nld -I batch_in -O batch_out folia1.xml >& uit2
cat < uit2
$exe -L nld -B -I batch_in -O batch_out folia1.xml >& uit3
cat < uit3
$exe -L nld -B -I batch_in -O batch_in >& uit4
cat < uit4
$exe -L fra -B -F -X test.fr.txt multilang.txt -O batch_out5 >&uit5
cat < uit5

echo "Step 4"
$exe -L nld -B -I batch_in -O batch_out3
$folialint --nooutput batch_out3/folia4.ucto.xml 2>&1
cat < batch_out3/testpunctuation.ucto.txt
$folialint --nooutput batch_out3/folia8.ucto.xml 2>&1

echo "Step 5"
$exe -L nld -B -F -I batch_in -O batch_out4 # force only .xml input
$folialint --nooutput batch_out4/folia4.ucto.xml 2>&1
cat < batch_out/testpunctuation.ucto.txt 2>&1 # must not be present!
$folialint --nooutput batch_out4/folia8.ucto.xml 2>&1

echo "Done"
