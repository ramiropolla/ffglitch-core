
FFEDIT_TESTS += png_lena_128_list
FFEDIT_TESTS += apng_wheel_list

png_lena_128_list: $(FFEDIT_SRC)/png_lena_128.png
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
apng_wheel_list: $(FFEDIT_SRC)/wheel.apng
	$(FFEDIT_SCRIPTS)/list.sh "$(FFEDIT_REF)/$@.ref" "$<"
