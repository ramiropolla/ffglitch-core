
FFGLITCH_TESTS += mpeg2video_traffic_128_5_list         \
                  mpeg2video_traffic_128_5_replicate    \
                  mpeg2video_traffic_128_5_info         \
                  mpeg2video_traffic_128_5_q_dct        \
                  mpeg2video_traffic_128_5_mv           \
                  mpeg2video_traffic_128_5_qscale       \
                  mpeg2video_traffic_128_5_mb           \
                  mpeg2video_prepared_128_5_info        \
                  mpeg2video_prepared_128_5_q_dct       \
                  mpeg2video_prepared_128_5_mv          \

mpeg2video_traffic_128_5_replicate: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_replicate.sh "$(CAS9_REF)/$@.ref" "$<"
mpeg2video_traffic_128_5_list: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_list.sh "$(CAS9_REF)/$@.ref" "$<"
mpeg2video_traffic_128_5_transplicate: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_transplicate.sh "$(CAS9_REF)/$@.ref" "$<"

mpeg2video_traffic_128_5_info: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f info" "cat.sh"
mpeg2video_traffic_128_5_q_dct: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f q_dct" "dct_ac_sorter.py"
mpeg2video_traffic_128_5_qscale: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f qscale" "qscale_max0.py"
mpeg2video_traffic_128_5_mb: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f mb" "mb_sorter.py"

mpeg2video_prepared_128_5_info: $(CAS9_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f info" "cat.sh"
mpeg2video_prepared_128_5_q_dct: $(CAS9_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f q_dct" "dct_ac_sorter.py"
mpeg2video_prepared_128_5_mv: $(CAS9_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f mv" "mv_sink_and_rise.py"
