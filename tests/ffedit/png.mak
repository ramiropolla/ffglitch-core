
FFEDIT_TESTS += png_lena_128_list
FFEDIT_TESTS += png_lena_128_replicate
FFEDIT_TESTS += apng_wheel_list
FFEDIT_TESTS += apng_wheel_replicate

png_lena_128_list: $(FFEDIT_SRC)/png_lena_128.png
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
png_lena_128_replicate: $(FFEDIT_SRC)/png_lena_128.png
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
apng_wheel_list: $(FFEDIT_SRC)/wheel.apng
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
apng_wheel_replicate: $(FFEDIT_SRC)/wheel.apng
	$(FFEDIT_SCRIPTS)/replicate.sh "$(FFEDIT_REF)/$@.ref" "$<"
