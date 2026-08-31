#include <SRT_FrameReceiver.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <chrono>
#include <format>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

SRT_FrameReceiver *SRT_FrameReceiver::s_instance = nullptr;
std::mutex SRT_FrameReceiver::s_mutex{};
std::atomic<bool> SRT_FrameReceiver::s_initialized{false};

namespace {

// ReSharper disable once CppParameterMayBeConstPtrOrRef
int ffmpegInterruptCallback(void *opaque) {
	const auto *stop = static_cast<const std::atomic<bool> *>(opaque);
	return (stop != nullptr && stop->load()) ? 1 : 0;
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

bool codecAllowed(const AVCodecID codecId) {
	switch (codecId) {
	case AV_CODEC_ID_H264:
	case AV_CODEC_ID_HEVC:
	case AV_CODEC_ID_AV1:
		return true;
	default:
		return false;
	}
}

} // namespace

bool SRT_FrameReceiver::geometryAllowed(const int width, const int height) {
	if (width <= 0 || height <= 0)
		return false;
	if (width > c_maxWidth || height > c_maxHeight)
		return false;
	return static_cast<int64_t>(width) * static_cast<int64_t>(height) <= c_maxPixels;
}

SRT_FrameReceiver::SRT_FrameReceiver() = default;

void SRT_FrameReceiver::init(const uint16_t port) {
	std::lock_guard<std::mutex> lock(s_mutex);
	if (s_instance) {
		return;
	}

	s_instance = new SRT_FrameReceiver();
	s_instance->m_port = port;
	s_initialized.store(true);
}

void SRT_FrameReceiver::connectReceiver(std::function<void(obs_source_frame)> &&frameCallback, std::string passphrase) {
	if (!s_initialized.load() || !s_instance) {
		obs_log(LOG_ERROR, "SRT_FrameReceiver:connectReceiver: SRT_FrameReceiver object not initialized");
		return;
	}

	if (passphrase.size() < c_minPassphraseLength) {
		obs_log(LOG_ERROR, "SRT_FrameReceiver:connectReceiver: passphrase must be at least %zu characters",
			c_minPassphraseLength);
		return;
	}

	std::lock_guard<std::mutex> lock(s_mutex);

	if (s_instance->m_active.load()) {
		s_instance->stopReceiver();
	}

	{
		std::lock_guard<std::mutex> callbackLock(s_instance->m_callbackMutex);
		s_instance->m_frameCallback = std::move(frameCallback);
	}

	s_instance->m_passphrase = std::move(passphrase);
	s_instance->m_active.store(true);
	s_instance->startReceiver();
}

void SRT_FrameReceiver::disconnectReceiver() {
	if (!s_initialized.load() || !s_instance) {
		obs_log(LOG_ERROR, "SRT_FrameReceiver:disconnectReceiver: SRT_FrameReceiver object not initialized");
		return;
	}

	std::lock_guard<std::mutex> lock(s_mutex);

	{
		std::lock_guard<std::mutex> callbackLock(s_instance->m_callbackMutex);
		s_instance->m_frameCallback = nullptr;
	}

	s_instance->m_active.store(false);
	s_instance->stopReceiver();
}

bool SRT_FrameReceiver::active() {
	if (!s_initialized.load() || !s_instance) {
		return false;
	}

	return s_instance->m_active.load();
}

void SRT_FrameReceiver::startReceiver() {
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
	m_avCodecContext.reset();
	m_avFormatContext.reset();
	m_swsContext.reset();
	m_scaledBuffer.reset();
	m_videoStreamIdx = -1;
}

void SRT_FrameReceiver::receiveThread(const std::stop_token &token) {
	const AVPacketPtr packet(av_packet_alloc());
	const AVFramePtr frame(av_frame_alloc());

	if (!packet || !frame) {
		obs_log(LOG_ERROR, "SRT_FrameReceiver: failed to allocate FFmpeg packet/frame");
		m_active.store(false);
		return;
	}

	while (!token.stop_requested()) {
		if (!m_avFormatContext || !m_avCodecContext) {
			closeStream();
			if (!openStream()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}
		}

		const int ret = av_read_frame(m_avFormatContext.get(), packet.get());
		if (ret < 0) {
			if (token.stop_requested()) {
				break;
			}

			obs_log(LOG_WARNING, "SRT_FrameReceiver: read error, reconnecting");
			closeStream();
			continue;
		}

		if (packet->stream_index == m_videoStreamIdx && m_avCodecContext) {
			if (avcodec_send_packet(m_avCodecContext.get(), packet.get()) == 0) {
				while (avcodec_receive_frame(m_avCodecContext.get(), frame.get()) == 0) {
					submitFrame(frame.get());
					av_frame_unref(frame.get());
				}
			}
		}

		av_packet_unref(packet.get());
	}
}

bool SRT_FrameReceiver::openStream() {
	if (m_passphrase.size() < c_minPassphraseLength) {
		obs_log(LOG_ERROR, "SRT_FrameReceiver: refusing to listen without a valid passphrase");
		return false;
	}

	AVFormatContext *avFormatContext = avformat_alloc_context();
	if (!avFormatContext) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: avformat_alloc_context failed");
		return false;
	}

	avFormatContext->interrupt_callback.callback = ffmpegInterruptCallback;
	avFormatContext->interrupt_callback.opaque = &m_interruptStop;

	AVDictionary *options = nullptr;
	av_dict_set(&options, "mode", "listener", 0);
	av_dict_set(&options, "passphrase", m_passphrase.c_str(), 0);
	av_dict_set(&options, "pbkeylen", "16", 0);
	av_dict_set(&options, "listen_timeout", "5000000", 0);
	av_dict_set(&options, "rw_timeout", "5000000", 0);

	const std::string url = std::format("srt://127.0.0.1:{}", m_port);
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
	for (unsigned i = 0; i < formatContext->nb_streams; i++) {
		if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStreamIdx = static_cast<int>(i);
			break;
		}
	}

	if (videoStreamIdx < 0) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: no video stream found in SRT payload");
		return false;
	}

	const AVCodecParameters *par = formatContext->streams[videoStreamIdx]->codecpar;
	if (!codecAllowed(par->codec_id)) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: rejected codec id %d", par->codec_id);
		return false;
	}

	if (!geometryAllowed(par->width, par->height)) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: rejected stream geometry %dx%d", par->width, par->height);
		return false;
	}

	const AVCodec *codec = avcodec_find_decoder(par->codec_id);
	if (!codec) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: unsupported codec id %d", par->codec_id);
		return false;
	}

	AVCodecContextPtr codecContext(avcodec_alloc_context3(codec));
	if (!codecContext) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: avcodec_alloc_context3 failed");
		return false;
	}

	if (avcodec_parameters_to_context(codecContext.get(), par) < 0) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: avcodec_parameters_to_context failed");
		return false;
	}

	codecContext->max_pixels = c_maxPixels;

	if (avcodec_open2(codecContext.get(), codec, nullptr) < 0) {
		obs_log(LOG_WARNING, "SRT_FrameReceiver: failed to open decoder %s", codec->name);
		return false;
	}

	m_avFormatContext = std::move(formatContext);
	m_avCodecContext = std::move(codecContext);
	m_videoStreamIdx = videoStreamIdx;

	obs_log(LOG_INFO, "SRT_FrameReceiver: connected, video codec: %s (%dx%d)", codec->name, par->width,
		par->height);

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
	callback(obsFrame);
}
