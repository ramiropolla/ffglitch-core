
FFGLITCH_TESTS += mjpeg_lena420_128_list        \
                  mjpeg_lena420_128_replicate   \
                  mjpeg_lena420_128_info        \
                  mjpeg_lena420_128_q_dct       \
                  mjpeg_lena420_128_q_dc        \
                  mjpeg_lena420_128_dqt         \
                  mjpeg_lena420_128_dht         \

mjpeg_lena420_128_replicate: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_replicate.sh "$(CAS9_REF)/$@.ref" "$<"
mjpeg_lena420_128_list: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_list.sh "$(CAS9_REF)/$@.ref" "$<"
mjpeg_lena420_128_transplicate: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_transplicate.sh "$(CAS9_REF)/$@.ref" "$<"

mjpeg_lena420_128_info: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f info" "cat.sh"
mjpeg_lena420_128_q_dct: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f q_dct" "dct_ac_sorter.py"
mjpeg_lena420_128_q_dc: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f q_dc" "dc_inverter.py"
mjpeg_lena420_128_dqt: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f dqt" "dqt_max0.py"
mjpeg_lena420_128_dht: $(CAS9_SRC)/mjpeg_lena420_128.jpg
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f dht" "cat.sh"
