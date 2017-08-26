
FFGLITCH_TESTS += mpeg2video_traffic_128_5_list
FFGLITCH_TESTS += mpeg2video_traffic_128_5_replicate
FFGLITCH_TESTS += mpeg2video_traffic_128_5_info
FFGLITCH_TESTS += mpeg2video_traffic_128_5_mv
FFGLITCH_TESTS += mpeg2video_prepared_128_5_list
FFGLITCH_TESTS += mpeg2video_prepared_128_5_replicate
FFGLITCH_TESTS += mpeg2video_prepared_128_5_info
FFGLITCH_TESTS += mpeg2video_prepared_128_5_mv

mpeg2video_traffic_128_5_list: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_list.sh "$(CAS9_REF)/$@.ref" "$<"
mpeg2video_traffic_128_5_replicate: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_replicate.sh "$(CAS9_REF)/$@.ref" "$<"
mpeg2video_traffic_128_5_info: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f info" "cat.sh"
mpeg2video_traffic_128_5_mv: $(CAS9_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f mv" "mv_sink_and_rise.py"

mpeg2video_prepared_128_5_list: $(CAS9_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_list.sh "$(CAS9_REF)/$@.ref" "$<"
mpeg2video_prepared_128_5_replicate: $(CAS9_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_replicate.sh "$(CAS9_REF)/$@.ref" "$<"
mpeg2video_prepared_128_5_info: $(CAS9_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f info" "cat.sh"
mpeg2video_prepared_128_5_mv: $(CAS9_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f mv" "mv_sink_and_rise.py"
