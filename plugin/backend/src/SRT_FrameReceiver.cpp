#include <SRT_FrameReceiver.h>

#include <algorithm>
#include <plugin-support.h>
#include <util/platform.h>
#include <media-io/video-io.h>

#include <chrono>
#include <cerrno>
#include <format>
#include <utility>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace {
constexpr auto c_reconnectRetryInterval = std::chrono::seconds(5);

// ReSharper disable once CppParameterMayBeConstPtrOrRef
int ffmpegInterruptCallback(void *opaque) {
	const auto *stop = static_cast<const std::atomic<bool> *>(opaque);
	return (stop != nullptr && stop->load()) ? 1 : 0;
}

void logFfmpegError(const int level, const char *action, const int errorCode) {
	char errorBuffer[AV_ERROR_MAX_STRING_SIZE] = {};
	av_strerror(errorCode, errorBuffer, sizeof(errorBuffer));
	obs_log(level, "SRT_FrameReceiver: %s: %s", action, errorBuffer);
}

video_format ffmpegToObsFormat(const AVPixelFormat format) {
	switch (format) {
	case AV_PIX_FMT_YUV420P:
		return VIDEO_FORMAT_I420;
	case AV_PIX_FMT_NV12:
		return VIDEO_FORMAT_NV12;
	case AV_PIX_FMT_YUYV422:
		return VIDEO_FORMAT_YUY2;
	case AV_PIX_FMT_UYVY422:
		return VIDEO_FORMAT_UYVY;
	default:
		return VIDEO_FORMAT_NONE;
	}
}

audio_format ffmpegToObsAudioFormat(const AVSampleFormat format) {
	switch (format) {
	case AV_SAMPLE_FMT_FLTP:
		return AUDIO_FORMAT_FLOAT_PLANAR;
	case AV_SAMPLE_FMT_FLT:
		return AUDIO_FORMAT_FLOAT;
	case AV_SAMPLE_FMT_S16P:
		return AUDIO_FORMAT_16BIT_PLANAR;
	case AV_SAMPLE_FMT_S16:
		return AUDIO_FORMAT_16BIT;
	default:
		return AUDIO_FORMAT_UNKNOWN;
	}
}

speaker_layout ffmpegToObsSpeakers(const int channels) {
	switch (channels) {
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
		return SPEAKERS_4POINT0;
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	default:
		return SPEAKERS_UNKNOWN;
	}
}

bool codecAllowed(const AVCodecID codecId) {
	return std::ranges::contains(c_videoCodecs, codecId);
}

bool audioCodecAllowed(const AVCodecID codecId) {
	return std::ranges::contains(c_audioCodecs, codecId);
}

const AVCodec *findSoftwareDecoder(const AVCodecID codecId) {
	const AVCodec *defaultDecoder = avcodec_find_decoder(codecId);
	if (defaultDecoder != nullptr && (defaultDecoder->capabilities & AV_CODEC_CAP_HARDWARE) == 0) {
		return defaultDecoder;
	}

	void *iterator = nullptr;
	while (const AVCodec *codec = av_codec_iterate(&iterator)) {
		if (av_codec_is_decoder(codec) && codec->id == codecId &&
		    (codec->capabilities & AV_CODEC_CAP_HARDWARE) == 0) {
			return codec;
		}
	}

	return defaultDecoder;
}

void setObsColorMetadata(obs_source_frame &obsFrame, const AVFrame *frame) {
	const video_colorspace colorSpace =
		frame->colorspace == AVCOL_SPC_SMPTE170M || frame->colorspace == AVCOL_SPC_BT470BG ? VIDEO_CS_601
												   : VIDEO_CS_709;
	const video_range_type range = frame->color_range == AVCOL_RANGE_JPEG ? VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;

	obsFrame.full_range = range == VIDEO_RANGE_FULL;
	(void)video_format_get_parameters_for_format(colorSpace, range, obsFrame.format, obsFrame.color_matrix,
						     obsFrame.color_range_min, obsFrame.color_range_max);
}

} // namespace

void ScaledBuffer::reset() {
	if (data[0])
		av_freep(&data[0]);

	data[1] = data[2] = data[3] = nullptr;
	lineSize[0] = lineSize[1] = lineSize[2] = lineSize[3] = 0;
	width = 0;
	height = 0;
	srcFormat = AV_PIX_FMT_NONE;
}

ScaledBuffer::~ScaledBuffer() {
	reset();
}

void ConvertedAudio::reset() {
	if (data[0])
		av_freep(&data[0]);

	for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
		data[i] = nullptr;
	}

	capacity = 0;
	channels = 0;
}

ConvertedAudio::~ConvertedAudio() {
	reset();
}

bool SRT_FrameReceiver::geometryAllowed(const int width, const int height) {
	if (width <= 0 || height <= 0)
		return false;

	if (width > c_maxWidth || height > c_maxHeight)
		return false;

	return static_cast<int64_t>(width) * static_cast<int64_t>(height) <= c_maxPixels;
}

bool SRT_FrameReceiver::isHardwareFrame(const AVFrame *frame) {
	if (!frame) {
		return false;
	}

	const auto format = static_cast<AVPixelFormat>(frame->format);
	const AVPixFmtDescriptor *descriptor = av_pix_fmt_desc_get(format);
	return frame->hw_frames_ctx != nullptr ||
	       (descriptor != nullptr && (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0);
}

AVPixelFormat SRT_FrameReceiver::selectHardwareFormat(AVCodecContext *codecContext, const AVPixelFormat *formats) {
	const auto *receiver = static_cast<const SRT_FrameReceiver *>(codecContext->opaque);
	if (receiver != nullptr && receiver->m_hardwarePixelFormat != AV_PIX_FMT_NONE) {
		for (const AVPixelFormat *format = formats; *format != AV_PIX_FMT_NONE; ++format) {
			if (*format == receiver->m_hardwarePixelFormat) {
				return *format;
			}
		}
	}

	for (const AVPixelFormat *format = formats; *format != AV_PIX_FMT_NONE; ++format) {
		const AVPixFmtDescriptor *descriptor = av_pix_fmt_desc_get(*format);
		if (descriptor != nullptr && (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL) == 0) {
			return *format;
		}
	}

	return AV_PIX_FMT_NONE;
}

void SRT_FrameReceiver::resetHardwareDecoder() {
	av_buffer_unref(&m_hwDeviceContext);
	m_hardwarePixelFormat = AV_PIX_FMT_NONE;
	m_usingHardwareDecoder = false;
	m_loggedHardwareFrameTransfer = false;
}

bool SRT_FrameReceiver::configureHardwareDecoder(AVCodecContext *codecContext, const AVCodec *codec) {
	resetHardwareDecoder();

	for (int configIndex = 0;; ++configIndex) {
		const AVCodecHWConfig *config = avcodec_get_hw_config(codec, configIndex);
		if (config == nullptr) {
			break;
		}

		if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) == 0 ||
		    config->device_type == AV_HWDEVICE_TYPE_NONE) {
			continue;
		}

		AVBufferRef *deviceContext = nullptr;
		const int ret = av_hwdevice_ctx_create(&deviceContext, config->device_type, nullptr, nullptr, 0);
		if (ret < 0) {
			const char *deviceName = av_hwdevice_get_type_name(config->device_type);
			char errorBuffer[AV_ERROR_MAX_STRING_SIZE] = {};
			av_strerror(ret, errorBuffer, sizeof(errorBuffer));
			obs_log(LOG_DEBUG, "SRT_FrameReceiver: could not initialize %s hardware device: %s",
				deviceName ? deviceName : "unknown", errorBuffer);
			continue;
		}

		AVBufferRef *decoderDeviceContext = av_buffer_ref(deviceContext);
		if (!decoderDeviceContext) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: could not retain hardware device context");
			av_buffer_unref(&deviceContext);
			continue;
		}

		m_hwDeviceContext = deviceContext;
		codecContext->hw_device_ctx = decoderDeviceContext;
		codecContext->opaque = this;
		codecContext->get_format = selectHardwareFormat;
		m_hardwarePixelFormat = config->pix_fmt;
		m_usingHardwareDecoder = true;

		const char *deviceName = av_hwdevice_get_type_name(config->device_type);
		obs_log(LOG_INFO, "SRT_FrameReceiver: using %s hardware decoding for %s",
			deviceName ? deviceName : "unknown", codec->name);

		return true;
	}

	return false;
}

bool SRT_FrameReceiver::transferHardwareFrame(AVFrame *input, AVFrame *&output) {
	output = input;
	if (!isHardwareFrame(input)) {
		return true;
	}

	if (!input->hw_frames_ctx) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: received a hardware frame without a transfer context");
		return false;
	}

	if (!m_loggedHardwareFrameTransfer) {
		const char *formatName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(input->format));
		obs_log(LOG_INFO, "SRT_FrameReceiver: transferring %s frames to CPU for OBS",
			formatName ? formatName : "hardware");
		m_loggedHardwareFrameTransfer = true;
	}

	if (!m_cpuTransferFrame) {
		m_cpuTransferFrame.reset(av_frame_alloc());
	}

	if (!m_cpuTransferFrame) {
		obs_log(LOG_ERROR, "SRT_FrameReceiver: failed to allocate CPU transfer frame");
		return false;
	}

	av_frame_unref(m_cpuTransferFrame.get());
	const int ret = av_hwframe_transfer_data(m_cpuTransferFrame.get(), input, 0);
	if (ret < 0) {
		logFfmpegError(LOG_WARNING, "hardware frame transfer to CPU failed", ret);
		return false;
	}

	if (const int propsRet = av_frame_copy_props(m_cpuTransferFrame.get(), input); propsRet < 0) {
		logFfmpegError(LOG_WARNING, "copying transferred frame properties failed", propsRet);
		av_frame_unref(m_cpuTransferFrame.get());
		return false;
	}

	output = m_cpuTransferFrame.get();
	return true;
}

SRT_FrameReceiver::SRT_FrameReceiver(const uint16_t port, std::string streamID,
				     std::map<AVCodecID, const AVCodec *> codecs)
	: m_streamID(std::move(streamID)),
	  m_codecs(std::move(codecs)),
	  m_port(port) {}

SRT_FrameReceiver::~SRT_FrameReceiver() {
	disconnectReceiver();
}

void SRT_FrameReceiver::connectReceiver(std::function<void(obs_source_frame)> &&frameCallback,
					std::function<void(obs_source_audio)> &&audioCallback) {

	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_active.load()) {
		stopReceiver();
	}

	{
		std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
		m_frameCallback = std::move(frameCallback);
		m_audioCallback = std::move(audioCallback);
	}

	m_active.store(true);
	startReceiver();
}

void SRT_FrameReceiver::disconnectReceiver() {
	std::lock_guard<std::mutex> lock(m_mutex);

	{
		std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
		m_frameCallback = nullptr;
		m_audioCallback = nullptr;
	}

	m_active.store(false);
	stopReceiver();
}

bool SRT_FrameReceiver::active() const {
	return m_active.load();
}

uint32_t SRT_FrameReceiver::getBitrate() {
	const uint64_t byteCount = m_bitCount.exchange(0);
	const uint64_t currentTime = os_gettime_ns();
	const uint64_t lastTime = m_bitrateLastCheck.exchange(currentTime);
	const uint64_t elapsedNs = currentTime - lastTime;

	if (elapsedNs == 0) {
		m_bitCount.fetch_add(byteCount);
		return 0;
	}

	return static_cast<uint32_t>((byteCount * 8 * 1000000000ULL) / elapsedNs);
}

void SRT_FrameReceiver::startReceiver() {
	m_forceSoftwareVideoDecoder = false;
	m_interruptStop.store(false);
	m_frameReceiverThread = std::jthread([this](const std::stop_token &token) { receiveThread(token); });
}

void SRT_FrameReceiver::stopReceiver() {
	m_interruptStop.store(true);
	m_frameReceiverThread.request_stop();

	if (m_frameReceiverThread.joinable()) {
		m_frameReceiverThread.join();
	}

	closeStream();
}

void SRT_FrameReceiver::closeStream() {
	m_cpuTransferFrame.reset();
	m_avCodecContext.reset();
	resetHardwareDecoder();
	m_avFormatContext.reset();
	m_avAudioCodecContext.reset();
	m_swsContext.reset();
	m_scaledBuffer.reset();
	m_swrContext.reset();
	m_convertedAudio.reset();
	m_swrSrcFormat = AV_SAMPLE_FMT_NONE;
	m_swrSrcRate = 0;
	m_swrSrcChannels = 0;
	m_videoStreamIdx = -1;
	m_audioStreamIdx = -1;
}

void SRT_FrameReceiver::receiveThread(const std::stop_token &token) {
	const AVPacketPtr packet(av_packet_alloc());
	const AVFramePtr frame(av_frame_alloc());

	if (!packet || !frame) {
		obs_log(LOG_ERROR, "SRT_FrameReceiver: failed to allocate FFmpeg packet/frame");
		m_active.store(false);
		return;
	}

	m_bitCount.store(0);
	m_bitrateLastCheck.store(os_gettime_ns());

	while (!token.stop_requested()) {
		if (!m_avFormatContext || !m_avCodecContext) {
			closeStream();
			if (!openStream()) {
				const auto retryAt = std::chrono::steady_clock::now() + c_reconnectRetryInterval;
				while (!token.stop_requested() && std::chrono::steady_clock::now() < retryAt) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
				continue;
			}
		}

		const int ret = av_read_frame(m_avFormatContext.get(), packet.get());
		if (ret < 0) {
			if (token.stop_requested()) {
				break;
			}

			obs_log(LOG_WARNING, "SRT_FrameReceiver: read error, reconnecting");
			flushVideoDecoder();
			flushAudioDecoder();
			closeStream();
			continue;
		}

		if (packet->size > 0) {
			m_bitCount.fetch_add(static_cast<uint64_t>(packet->size));
		}

		if (packet->stream_index == m_videoStreamIdx && m_avCodecContext) {
			bool reconnectWithSoftwareDecoder = false;
			const int sendRet = avcodec_send_packet(m_avCodecContext.get(), packet.get());
			if (sendRet < 0) {
				logFfmpegError(LOG_WARNING, "sending video packet to decoder failed", sendRet);
			} else {
				for (;;) {
					const int receiveRet =
						avcodec_receive_frame(m_avCodecContext.get(), frame.get());
					if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
						break;
					}

					if (receiveRet < 0) {
						logFfmpegError(LOG_WARNING, "receiving video frame from decoder failed",
							       receiveRet);
						break;
					}

					submitFrame(frame.get());
					av_frame_unref(frame.get());

					if (m_forceSoftwareVideoDecoder) {
						reconnectWithSoftwareDecoder = true;
						break;
					}
				}
			}

			if (reconnectWithSoftwareDecoder) {
				obs_log(LOG_WARNING,
					"SRT_FrameReceiver: hardware frame transfer failed; reconnecting with software decoding");
				av_packet_unref(packet.get());
				closeStream();
				continue;
			}

		} else if (packet->stream_index == m_audioStreamIdx && m_avAudioCodecContext) {
			const int sendRet = avcodec_send_packet(m_avAudioCodecContext.get(), packet.get());
			if (sendRet < 0) {
				logFfmpegError(LOG_WARNING, "sending audio packet to decoder failed", sendRet);
			} else {
				for (;;) {
					const int receiveRet =
						avcodec_receive_frame(m_avAudioCodecContext.get(), frame.get());
					if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
						break;
					}

					if (receiveRet < 0) {
						logFfmpegError(LOG_WARNING, "receiving audio frame from decoder failed",
							       receiveRet);
						break;
					}

					submitAudio(frame.get());
					av_frame_unref(frame.get());
				}
			}
		}

		av_packet_unref(packet.get());
	}
}

void SRT_FrameReceiver::flushVideoDecoder() {
	if (!m_avCodecContext) {
		return;
	}

	const int sendRet = avcodec_send_packet(m_avCodecContext.get(), nullptr);
	if (sendRet < 0 && sendRet != AVERROR_EOF) {
		logFfmpegError(LOG_WARNING, "flushing video decoder failed", sendRet);
		return;
	}

	const AVFramePtr frame(av_frame_alloc());
	if (!frame) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: failed to allocate video flush frame");
		return;
	}

	for (;;) {
		const int receiveRet = avcodec_receive_frame(m_avCodecContext.get(), frame.get());
		if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
			break;
		}

		if (receiveRet < 0) {
			logFfmpegError(LOG_WARNING, "receiving flushed video frame failed", receiveRet);
			break;
		}

		submitFrame(frame.get());
		av_frame_unref(frame.get());
	}
}

void SRT_FrameReceiver::flushAudioDecoder() {
	if (!m_avAudioCodecContext) {
		return;
	}

	const int sendRet = avcodec_send_packet(m_avAudioCodecContext.get(), nullptr);
	if (sendRet < 0 && sendRet != AVERROR_EOF) {
		logFfmpegError(LOG_WARNING, "flushing audio decoder failed", sendRet);
		return;
	}

	const AVFramePtr frame(av_frame_alloc());
	if (!frame) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: failed to allocate audio flush frame");
		return;
	}

	for (;;) {
		const int receiveRet = avcodec_receive_frame(m_avAudioCodecContext.get(), frame.get());
		if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
			break;
		}

		if (receiveRet < 0) {
			logFfmpegError(LOG_WARNING, "receiving flushed audio frame failed", receiveRet);
			break;
		}

		submitAudio(frame.get());
		av_frame_unref(frame.get());
	}
}

bool SRT_FrameReceiver::openVideoDecoder(const AVCodecParameters *parameters) {
	const auto selectedCodecIt = m_codecs.find(parameters->codec_id);
	if (selectedCodecIt == m_codecs.end() || selectedCodecIt->second == nullptr) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: unsupported video codec id %d", parameters->codec_id);
		return false;
	}

	auto createContext = [this, parameters](const AVCodec *codec, const bool enableHardware) -> AVCodecContextPtr {
		AVCodecContextPtr codecContext(avcodec_alloc_context3(codec));
		if (!codecContext) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: avcodec_alloc_context3 failed");
			return {};
		}

		const int parametersRet = avcodec_parameters_to_context(codecContext.get(), parameters);
		if (parametersRet < 0) {
			logFfmpegError(LOG_WARNING, "copying video codec parameters failed", parametersRet);
			return {};
		}

		codecContext->max_pixels = c_maxPixels;
		if (enableHardware) {
			(void)configureHardwareDecoder(codecContext.get(), codec);
		} else {
			resetHardwareDecoder();
		}

		const int openRet = avcodec_open2(codecContext.get(), codec, nullptr);
		if (openRet < 0) {
			logFfmpegError(LOG_WARNING, "opening video decoder failed", openRet);
			codecContext.reset();
			resetHardwareDecoder();
			return {};
		}

		return codecContext;
	};

	const AVCodec *codec = selectedCodecIt->second;
	AVCodecContextPtr codecContext;
	if (!m_forceSoftwareVideoDecoder) {
		codecContext = createContext(codec, true);
	}

	if (!codecContext) {
		const AVCodec *softwareCodec = findSoftwareDecoder(parameters->codec_id);
		if (!softwareCodec) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: no software fallback for video codec id %d",
				parameters->codec_id);
			return false;
		}

		if (softwareCodec != codec) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: falling back from decoder %s to %s", codec->name,
				softwareCodec->name);
		}

		codec = softwareCodec;
		codecContext = createContext(codec, false);
		if (!codecContext) {
			return false;
		}
	}

	m_avCodecContext = std::move(codecContext);
	obs_log(LOG_INFO, "SRT_FrameReceiver: connected, video codec: %s (%dx%d)%s", codec->name, parameters->width,
		parameters->height, m_usingHardwareDecoder ? " with hardware acceleration" : "");
	return true;
}

bool SRT_FrameReceiver::openStream() {
	AVFormatContext *avFormatContext = avformat_alloc_context();
	if (!avFormatContext) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: avformat_alloc_context failed");
		return false;
	}

	avFormatContext->interrupt_callback.callback = ffmpegInterruptCallback;
	avFormatContext->interrupt_callback.opaque = &m_interruptStop;

	AVDictionary *options = nullptr;
	av_dict_set(&options, "mode", "caller", 0);
	av_dict_set(&options, "listen_timeout", "5000000", 0);
	av_dict_set(&options, "rw_timeout", "5000000", 0);

	const std::string url = std::format("srt://127.0.0.1:{}?streamid={}", m_port, m_streamID);
	const int ret = avformat_open_input(&avFormatContext, url.c_str(), nullptr, &options);
	av_dict_free(&options);

	if (ret < 0) {
		char errorBuffer[128];
		av_strerror(ret, errorBuffer, sizeof(errorBuffer));
		obs_log(LOG_WARNING, "SRT_FrameReceiver: avformat_open_input failed: %s", errorBuffer);
		return false;
	}

	AVFormatContextPtr formatContext(avFormatContext);

	if (avformat_find_stream_info(formatContext.get(), nullptr) < 0) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: could not find stream info");
		return false;
	}

	int videoStreamIdx = -1;
	int audioStreamIdx = -1;

	for (unsigned int i = 0; i < formatContext->nb_streams && (videoStreamIdx == -1 || audioStreamIdx == -1); i++) {
		if (videoStreamIdx == -1 && formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStreamIdx = static_cast<int>(i);
			continue;
		}

		if (audioStreamIdx == -1 && formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStreamIdx = static_cast<int>(i);
		}
	}

	if (videoStreamIdx < 0) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: no video stream found in SRT payload");
		return false;
	}

	if (audioStreamIdx < 0) {
		obs_log(LOG_WARNING,
			"SRT_FrameReceiver: no audio stream found in SRT payload, continuing with video-only mode");
	}

	{
		const AVCodecParameters *par = formatContext->streams[videoStreamIdx]->codecpar;
		if (!codecAllowed(par->codec_id)) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: rejected codec id %d", par->codec_id);
			return false;
		}

		if (!geometryAllowed(par->width, par->height)) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: rejected stream geometry %dx%d", par->width,
				par->height);
			return false;
		}

		if (!openVideoDecoder(par)) {
			return false;
		}

		m_videoStreamIdx = videoStreamIdx;
	}

	if (audioStreamIdx >= 0) {
		const AVCodecParameters *par = formatContext->streams[audioStreamIdx]->codecpar;
		if (!audioCodecAllowed(par->codec_id)) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: rejected audio codec id %d", par->codec_id);
			goto finish;
		}

		const auto selectedCodecIt = m_codecs.find(par->codec_id);
		if (selectedCodecIt == m_codecs.end() || selectedCodecIt->second == nullptr) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: unsupported audio codec id %d", par->codec_id);
			goto finish;
		}
		const AVCodec *codec = selectedCodecIt->second;

		AVCodecContextPtr codecContext(avcodec_alloc_context3(codec));
		if (!codecContext) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: avcodec_alloc_context3 failed");
			goto finish;
		}

		if (avcodec_parameters_to_context(codecContext.get(), par) < 0) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: avcodec_parameters_to_context failed");
			goto finish;
		}

		codecContext->max_pixels = c_maxPixels;

		if (avcodec_open2(codecContext.get(), codec, nullptr) < 0) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: failed to open audio decoder %s", codec->name);
			goto finish;
		}

		m_avAudioCodecContext = std::move(codecContext);
		m_audioStreamIdx = audioStreamIdx;
	}

finish:
	m_avFormatContext = std::move(formatContext);
	return true;
}

void SRT_FrameReceiver::submitFrame(AVFrame *frame) {
	if (!geometryAllowed(frame->width, frame->height)) {
		return;
	}

	std::function<void(obs_source_frame)> callback;
	{
		std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
		callback = m_frameCallback;
	}

	if (!callback) {
		return;
	}

	AVFrame *cpuFrame = nullptr;
	if (!transferHardwareFrame(frame, cpuFrame)) {
		m_forceSoftwareVideoDecoder = true;
		return;
	}
	frame = cpuFrame;

	obs_source_frame obsFrame = {};
	obsFrame.width = frame->width;
	obsFrame.height = frame->height;
	obsFrame.timestamp = os_gettime_ns();

	const auto srcFormat = static_cast<AVPixelFormat>(frame->format);
	const video_format directFmt = ffmpegToObsFormat(srcFormat);

	if (directFmt != VIDEO_FORMAT_NONE) {
		for (int i = 0; i < MAX_AV_PLANES; i++) {
			obsFrame.data[i] = frame->data[i];
			obsFrame.linesize[i] = static_cast<uint32_t>(frame->linesize[i]);
		}

		obsFrame.format = directFmt;
		setObsColorMetadata(obsFrame, frame);
		callback(obsFrame);
		return;
	}

	const bool scalerStale = !m_swsContext || m_scaledBuffer.width != frame->width ||
				 m_scaledBuffer.height != frame->height || m_scaledBuffer.srcFormat != srcFormat;
	if (scalerStale) {
		m_swsContext.reset(sws_getContext(frame->width, frame->height, srcFormat, frame->width, frame->height,
						  AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr));
		if (!m_swsContext) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: sws_getContext failed");
			m_scaledBuffer.reset();
			return;
		}

		m_scaledBuffer.reset();
		if (av_image_alloc(m_scaledBuffer.data, m_scaledBuffer.lineSize, frame->width, frame->height,
				   AV_PIX_FMT_YUV420P, 1) < 0) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: av_image_alloc failed");
			m_swsContext.reset();
			return;
		}

		m_scaledBuffer.width = frame->width;
		m_scaledBuffer.height = frame->height;
		m_scaledBuffer.srcFormat = srcFormat;
	}

	if (!m_swsContext || !m_scaledBuffer.data[0]) {
		return;
	}

	if (sws_scale(m_swsContext.get(), frame->data, frame->linesize, 0, frame->height, m_scaledBuffer.data,
		      m_scaledBuffer.lineSize) < 1) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: sws_scale failed");
		return;
	}

	for (int i = 0; i < 3; i++) {
		obsFrame.data[i] = m_scaledBuffer.data[i];
		obsFrame.linesize[i] = static_cast<uint32_t>(m_scaledBuffer.lineSize[i]);
	}

	obsFrame.format = VIDEO_FORMAT_I420;
	setObsColorMetadata(obsFrame, frame);
	callback(obsFrame);
}

void SRT_FrameReceiver::submitAudio(AVFrame *frame) {
	std::function<void(obs_source_audio)> callback;
	{
		std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
		callback = m_audioCallback;
	}

	if (!callback) {
		return;
	}

	const speaker_layout speakers = ffmpegToObsSpeakers(frame->ch_layout.nb_channels);
	if (speakers == SPEAKERS_UNKNOWN) {
		return;
	}

	obs_source_audio obsAudio = {};
	obsAudio.speakers = speakers;
	obsAudio.timestamp = os_gettime_ns();
	obsAudio.samples_per_sec = frame->sample_rate;

	const auto srcFormat = static_cast<AVSampleFormat>(frame->format);
	const audio_format directFmt = ffmpegToObsAudioFormat(srcFormat);

	if (directFmt != AUDIO_FORMAT_UNKNOWN) {
		obsAudio.frames = frame->nb_samples;
		obsAudio.format = directFmt;
		for (int i = 0; i < MAX_AV_PLANES; i++)
			obsAudio.data[i] = frame->data[i];
		callback(obsAudio);
		return;
	}

	const bool converterStale = !m_swrContext || m_swrSrcFormat != srcFormat ||
				    m_swrSrcRate != frame->sample_rate ||
				    m_swrSrcChannels != frame->ch_layout.nb_channels;
	if (converterStale) {
		m_swrContext.reset();
		m_convertedAudio.reset();

		SwrContext *swr = nullptr;
		if (swr_alloc_set_opts2(&swr, &frame->ch_layout, AV_SAMPLE_FMT_FLTP, frame->sample_rate,
					&frame->ch_layout, srcFormat, frame->sample_rate, 0, nullptr) < 0 ||
		    swr_init(swr) < 0) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: swr_alloc_set_opts2/swr_init failed");
			swr_free(&swr);
			return;
		}

		m_swrContext.reset(swr);
		m_swrSrcFormat = srcFormat;
		m_swrSrcRate = frame->sample_rate;
		m_swrSrcChannels = frame->ch_layout.nb_channels;
	}

	const int outCapacity = swr_get_out_samples(m_swrContext.get(), frame->nb_samples);
	if (outCapacity <= 0) {
		return;
	}

	if (m_convertedAudio.capacity < outCapacity || m_convertedAudio.channels != frame->ch_layout.nb_channels) {
		m_convertedAudio.reset();
		if (av_samples_alloc(m_convertedAudio.data, nullptr, frame->ch_layout.nb_channels, outCapacity,
				     AV_SAMPLE_FMT_FLTP, 0) < 0) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: av_samples_alloc failed");
			return;
		}

		m_convertedAudio.capacity = outCapacity;
		m_convertedAudio.channels = frame->ch_layout.nb_channels;
	}

	const uint8_t *in[AV_NUM_DATA_POINTERS];
	for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
		in[i] = frame->extended_data[i];

	const int converted = swr_convert(m_swrContext.get(), m_convertedAudio.data, m_convertedAudio.capacity, in,
					  frame->nb_samples);
	if (converted <= 0) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: swr_convert failed");
		return;
	}

	obsAudio.frames = converted;
	obsAudio.format = AUDIO_FORMAT_FLOAT_PLANAR;
	for (int i = 0; i < MAX_AV_PLANES; i++) {
		obsAudio.data[i] = m_convertedAudio.data[i];
	}

	callback(obsAudio);
}
