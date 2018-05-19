
FFEDIT_TESTS += mjpeg_lena420_128_list
FFEDIT_TESTS += mjpeg_lena420_128_replicate

mjpeg_lena420_128_list: $(FFEDIT_SRC)/mjpeg_lena420_128.jpg
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
mjpeg_lena420_128_replicate: $(FFEDIT_SRC)/mjpeg_lena420_128.jpg
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
