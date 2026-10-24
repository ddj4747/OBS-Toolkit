#include <FFmpegCodecUtils.h>

#include <algorithm>
#include <iterator>
#include <string_view>

namespace {
constexpr std::string_view c_preferredDecoders[] = {
	// NVIDIA CUVID/NVDEC
	"h264_cuvid",
	"hevc_cuvid",
	"av1_cuvid",
	"vp9_cuvid",
	"vp8_cuvid",
	"mpeg4_cuvid",
	"mjpeg_cuvid",
	// AMD Advanced Media Framework
	"h264_amf",
	"hevc_amf",
	"av1_amf",
	"vp9_amf",
	"vp8_amf",
	"mpeg4_amf",
	"mjpeg_amf",
	// Intel Quick Sync Video
	"h264_qsv",
	"hevc_qsv",
	"av1_qsv",
	"vp9_qsv",
	"vp8_qsv",
	"mpeg4_qsv",
	"mjpeg_qsv",
	// Rockchip Media Process Platform
	"h264_rkmpp",
	"hevc_rkmpp",
	"av1_rkmpp",
	"vp9_rkmpp",
	"vp8_rkmpp",
	"mpeg4_rkmpp",
	"mjpeg_rkmpp",
	// Linux Video4Linux memory-to-memory
	"h264_v4l2m2m",
	"hevc_v4l2m2m",
	"av1_v4l2m2m",
	"vp9_v4l2m2m",
	"vp8_v4l2m2m",
	"mpeg4_v4l2m2m",
	"mjpeg_v4l2m2m",
	// Android MediaCodec
	"h264_mediacodec",
	"hevc_mediacodec",
	"av1_mediacodec",
	"vp9_mediacodec",
	"vp8_mediacodec",
	"mpeg4_mediacodec",
	"mjpeg_mediacodec",
	// Apple VideoToolbox builds that expose named decoders
	"h264_videotoolbox",
	"hevc_videotoolbox",
	"av1_videotoolbox",
	"vp9_videotoolbox",
	// Raspberry Pi MMAL
	"h264_mmal",
	"mpeg4_mmal",
	"mjpeg_mmal",
	// Preferred audio decoder implementations
	"libfdk_aac",
	"aac",
	"aac_fixed",
	"libopus",
	"opus",
	"mp3float",
	"mp3",
	"flac",
	"alac",
	"ac3",
	"ac3_fixed",
	"eac3",
	"libvorbis",
	"vorbis",
	"pcm_s16le",
	"pcm_s24le",
};

std::size_t preferredDecoderRank(const DecoderInfo &decoder) {
	const auto position = std::ranges::find(c_preferredDecoders, decoder.name);
	if (position == std::end(c_preferredDecoders))
		return std::size(c_preferredDecoders);

	return static_cast<std::size_t>(std::distance(std::begin(c_preferredDecoders), position));
}

bool canOpenDecoder(const AVCodec *codec) {
	if (!codec) {
		return false;
	}

	AVCodecContext *ctx = avcodec_alloc_context3(codec);
	if (!ctx)
		return false;

	const int ret = avcodec_open2(ctx, codec, nullptr);
	avcodec_free_context(&ctx);
	return ret == 0;
}

} // namespace

std::vector<DecoderInfo> findDecoders(const AVCodecID codecId) {
	std::vector<DecoderInfo> result;

	void *iterator = nullptr;
	const AVCodec *codec = nullptr;

	while ((codec = av_codec_iterate(&iterator)) != nullptr) {
		if (!av_codec_is_decoder(codec) || codec->id != codecId)
			continue;

		if (!canOpenDecoder(codec)) {
			continue;
		}

		DecoderInfo info{
			.codec = codec,
			.name = codec->name ? codec->name : "",
			.description = codec->long_name ? codec->long_name : "",
		};

		result.push_back(std::move(info));
	}

	std::ranges::stable_sort(result, [](const DecoderInfo &left, const DecoderInfo &right) {
		return preferredDecoderRank(left) < preferredDecoderRank(right);
	});

	return result;
}
