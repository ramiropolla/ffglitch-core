
FFEDIT_TESTS += mpeg4_prepared_128_5_list
FFEDIT_TESTS += mpeg4_prepared_128_5_replicate
FFEDIT_TESTS += mpeg4_prepared_128_5_bf_replicate
FFEDIT_TESTS += mpeg4_prepared_128_5_ilme_replicate
FFEDIT_TESTS += mpeg4_prepared_128_5_info

mpeg4_prepared_128_5_list: $(FFEDIT_SRC)/mpeg4_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg4_prepared_128_5_replicate: $(FFEDIT_SRC)/mpeg4_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg4_prepared_128_5_bf_replicate: $(FFEDIT_SRC)/mpeg4_prepared_128_5_bf.rawvideo
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg4_prepared_128_5_ilme_replicate: $(FFEDIT_SRC)/mpeg4_prepared_128_5_ilme.rawvideo
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
mpeg4_prepared_128_5_info: $(FFEDIT_SRC)/mpeg4_prepared_128_5.rawvideo
	$(FFEDIT_SCRIPTS)/script.sh "$(FFEDIT_REF)/$@.ref" "$<" "$(FFEDIT_JSON)/$@.json" "-f info" "cat.sh"
