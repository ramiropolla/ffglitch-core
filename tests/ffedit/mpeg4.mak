
FFEDIT_TESTS += mpeg4_prepared_128_5_list
FFEDIT_TESTS += mpeg4_prepared_128_5_replicate

mpeg4_prepared_128_5_list: $(FFEDIT_SRC)/mpeg4_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg4_prepared_128_5_replicate: $(FFEDIT_SRC)/mpeg4_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
