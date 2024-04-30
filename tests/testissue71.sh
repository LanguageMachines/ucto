#/bin/sh

$exe issue71_b.xml testissue71_b_1_out.xml --passthru
$folialint --nooutput testissue71_b_1_out.xml 2>&1
$exe issue71_b.xml testissue71_b_2_out.xml -L nld
$folialint --nooutput testissue71_b_2_out.xml 2>&1

$exe issue71_c.xml testissue71_c_1_out.xml --passthru
$folialint --nooutput testissue71_c_1_out.xml 2>&1
$exe issue71_c.xml testissue71_c_2_out.xml -L nld
$folialint --nooutput testissue71_c_2_out.xml 2>&1

$exe issue71_d.xml testissue71_d_1_out.xml --passthru
$folialint --nooutput testissue71_d_1_out.xml 2>&1
$exe issue71_d.xml testissue71_d_2_out.xml -L nld
$folialint --nooutput testissue71_d_2_out.xml 2>&1

$exe issue71_e.xml testissue71_e_1_out.xml --passthru
$folialint --nooutput testissue71_e_1_out.xml 2>&1
$exe issue71_e.xml testissue71_e_2_out.xml -L nld
$folialint --nooutput testissue71_e_2_out.xml 2>&1
