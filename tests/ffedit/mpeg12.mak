
FFEDIT_TESTS += mpeg2video_traffic_128_5_list
FFEDIT_TESTS += mpeg2video_traffic_128_5_replicate
FFEDIT_TESTS += mpeg2video_traffic_128_5_info
FFEDIT_TESTS += mpeg2video_traffic_128_5_mv
FFEDIT_TESTS += mpeg2video_prepared_128_5_list
FFEDIT_TESTS += mpeg2video_prepared_128_5_replicate
FFEDIT_TESTS += mpeg2video_prepared_128_5_info
FFEDIT_TESTS += mpeg2video_prepared_128_5_mv

mpeg2video_traffic_128_5_list: $(FFEDIT_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg2video_traffic_128_5_replicate: $(FFEDIT_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg2video_traffic_128_5_info: $(FFEDIT_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/script.sh "$(FFEDIT_REF)/$@.ref" "$<" "$(FFEDIT_JSON)/$@.json" "-f info" "cat.sh"
mpeg2video_traffic_128_5_mv: $(FFEDIT_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/script.sh "$(FFEDIT_REF)/$@.ref" "$<" "$(FFEDIT_JSON)/$@.json" "-f mv" "mv_sink_and_rise.py"

mpeg2video_prepared_128_5_list: $(FFEDIT_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg2video_prepared_128_5_replicate: $(FFEDIT_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg2video_prepared_128_5_info: $(FFEDIT_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/script.sh "$(FFEDIT_REF)/$@.ref" "$<" "$(FFEDIT_JSON)/$@.json" "-f info" "cat.sh"
mpeg2video_prepared_128_5_mv: $(FFEDIT_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/script.sh "$(FFEDIT_REF)/$@.ref" "$<" "$(FFEDIT_JSON)/$@.json" "-f mv" "mv_sink_and_rise.py"
