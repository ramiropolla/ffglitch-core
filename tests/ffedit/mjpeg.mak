
FFEDIT_TESTS += mjpeg_lena420_128_list
FFEDIT_TESTS += mjpeg_lena420_128_replicate
FFEDIT_TESTS += mjpeg_lena420_128_info

mjpeg_lena420_128_list: $(FFEDIT_SRC)/mjpeg_lena420_128.jpg
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
mjpeg_lena420_128_replicate: $(FFEDIT_SRC)/mjpeg_lena420_128.jpg
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
mjpeg_lena420_128_info: $(FFEDIT_SRC)/mjpeg_lena420_128.jpg
	$(FFEDIT_SCRIPTS)/script.sh "$(FFEDIT_REF)/$@.ref" "$<" "$(FFEDIT_JSON)/$@.json" "-f info" "cat.sh"
