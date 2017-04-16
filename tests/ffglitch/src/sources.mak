
FFGLITCH_TEST_SOURCES += mpeg2video_traffic3.rawvideo

$(addprefix $(SRC_PATH)/tests/ffglitch/src/,$(FFGLITCH_TEST_SOURCES)):
	curl -o $@ "http://ffglitch.org/pub/testdata/"$(notdir $@)

CAS9_SRC=$(SRC_PATH)/tests/ffglitch/src
