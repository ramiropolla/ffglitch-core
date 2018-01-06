
FFGLITCH_TESTS += mpeg4_prepared_128_5_list
FFGLITCH_TESTS += mpeg4_prepared_128_5_replicate

mpeg4_prepared_128_5_list: $(CAS9_SRC)/mpeg4_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_list.sh "$(CAS9_REF)/$@.ref" "$<"
mpeg4_prepared_128_5_replicate: $(CAS9_SRC)/mpeg4_prepared_128_5.rawvideo
	$(CAS9_SCRIPTS)/cas9_replicate.sh "$(CAS9_REF)/$@.ref" "$<"
