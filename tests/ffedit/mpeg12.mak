
FFEDIT_TESTS += mpeg2video_traffic_128_5_list
FFEDIT_TESTS += mpeg2video_traffic_128_5_replicate
FFEDIT_TESTS += mpeg2video_prepared_128_5_list
FFEDIT_TESTS += mpeg2video_prepared_128_5_replicate

mpeg2video_traffic_128_5_list: $(FFEDIT_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg2video_traffic_128_5_replicate: $(FFEDIT_SRC)/mpeg2video_traffic_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"

mpeg2video_prepared_128_5_list: $(FFEDIT_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg2video_prepared_128_5_replicate: $(FFEDIT_SRC)/mpeg2video_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
