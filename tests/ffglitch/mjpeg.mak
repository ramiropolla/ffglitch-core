
FFGLITCH_TESTS += mjpeg_lena420_128_list
FFGLITCH_TESTS += mjpeg_lena420_128_replicate
FFGLITCH_TESTS += mjpeg_lena420_128_info
FFGLITCH_TESTS += mjpeg_lena420_128_q_dct

mjpeg_lena420_128_list: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_list.sh "$(CAS9_REF)/$@.ref" "$<"
mjpeg_lena420_128_replicate: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_replicate.sh "$(CAS9_REF)/$@.ref" "$<"
mjpeg_lena420_128_info: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f info" "cat.sh"
mjpeg_lena420_128_q_dct: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f q_dct" "dct_ac_sorter.py"
