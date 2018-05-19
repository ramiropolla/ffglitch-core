
FFEDIT_TEST_SOURCES += mpeg2video_traffic3.rawvideo

$(addprefix $(SRC_PATH)/tests/ffedit/src/,$(FFEDIT_TEST_SOURCES)):
	curl -o $@ "http://ffglitch.org/pub/testdata/"$(notdir $@)

FFEDIT_SRC=$(SRC_PATH)/tests/ffedit/src
