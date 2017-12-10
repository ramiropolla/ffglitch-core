
FFGLITCH_TESTS += mpeg4_prepared_128_5_list
FFGLITCH_TESTS += mpeg4_prepared_128_5_replicate
FFGLITCH_TESTS += mpeg4_prepared_128_5_info
FFGLITCH_TESTS += mpeg4_prepared_128_5_mv

mpeg4_prepared_128_5_list: $(CAS9_SRC)/mpeg4_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_list.sh "$(CAS9_REF)/$@.ref" "$<"
mpeg4_prepared_128_5_replicate: $(CAS9_SRC)/mpeg4_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_replicate.sh "$(CAS9_REF)/$@.ref" "$<"
mpeg4_prepared_128_5_info: $(CAS9_SRC)/mpeg4_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f info" "cat.sh"
mpeg4_prepared_128_5_mv: $(CAS9_SRC)/mpeg4_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_script.sh "$(CAS9_REF)/$@.ref" "$<" "$(CAS9_JSON)/$@.json" "-f mv" "mv_sink_and_rise.py"
