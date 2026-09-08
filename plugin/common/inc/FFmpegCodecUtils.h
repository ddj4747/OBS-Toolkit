#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <string>
#include <vector>

struct DecoderInfo {
	const AVCodec *codec;
	std::string name;
	std::string description;
};

std::vector<DecoderInfo> findDecoders(AVCodecID codecId);
