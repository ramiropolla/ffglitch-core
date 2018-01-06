
$(addprefix $(SRC_PATH)/tests/ffedit/src/,$(FFEDIT_TEST_SOURCES)):
	curl -o $@ "http://ffglitch.org/pub/testdata/"$(notdir $@)

FFEDIT_SRC=$(SRC_PATH)/tests/ffedit/src
