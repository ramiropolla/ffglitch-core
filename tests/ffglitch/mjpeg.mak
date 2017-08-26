
FFGLITCH_TESTS += mjpeg_lena420_128_list
FFGLITCH_TESTS += mjpeg_lena420_128_replicate

mjpeg_lena420_128_list: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_list.sh "$(CAS9_REF)/$@.ref" "$<"
mjpeg_lena420_128_replicate: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_replicate.sh "$(CAS9_REF)/$@.ref" "$<"
