#include <SRT_FrameReceiver.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <chrono>
#include <format>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

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
	switch (codecId) {
	case AV_CODEC_ID_H264:
	case AV_CODEC_ID_HEVC:
	case AV_CODEC_ID_AV1:
		return true;
	default:
		return false;
	}
}

bool audioCodecAllowed(const AVCodecID codecId) {
	switch (codecId) {
	case AV_CODEC_ID_AAC:
	case AV_CODEC_ID_OPUS:
	case AV_CODEC_ID_MP3:
		return true;
	default:
		return false;
	}
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

SRT_FrameReceiver::SRT_FrameReceiver(const uint16_t port) : m_port(port) {}

SRT_FrameReceiver::~SRT_FrameReceiver() {
	disconnectReceiver();
}

void SRT_FrameReceiver::connectReceiver(std::function<void(obs_source_frame)> &&frameCallback,
					std::function<void(obs_source_audio)> &&audioCallback, std::string passphrase) {
	if (passphrase.size() < c_minPassphraseLength) {
		obs_log(LOG_ERROR, "SRT_FrameReceiver:connectReceiver: passphrase must be at least %zu characters",
			c_minPassphraseLength);
		return;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_active.load()) {
		stopReceiver();
	}

	{
		std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
		m_frameCallback = std::move(frameCallback);
		m_audioCallback = std::move(audioCallback);
	}

	m_passphrase = std::move(passphrase);
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

		if (packet->size > 0) {
			m_bitCount.fetch_add(static_cast<uint64_t>(packet->size));
		}

		if (packet->stream_index == m_videoStreamIdx && m_avCodecContext) {
			if (avcodec_send_packet(m_avCodecContext.get(), packet.get()) == 0) {
				while (avcodec_receive_frame(m_avCodecContext.get(), frame.get()) == 0) {
					submitFrame(frame.get());
					av_frame_unref(frame.get());
				}
			}

		} else if (packet->stream_index == m_audioStreamIdx && m_avAudioCodecContext) {
			if (avcodec_send_packet(m_avAudioCodecContext.get(), packet.get()) == 0) {
				while (avcodec_receive_frame(m_avAudioCodecContext.get(), frame.get()) == 0) {
					submitAudio(frame.get());
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

		m_avCodecContext = std::move(codecContext);
		m_videoStreamIdx = videoStreamIdx;

		obs_log(LOG_INFO, "SRT_FrameReceiver: connected, video codec: %s (%dx%d)", codec->name, par->width,
			par->height);
	}

	if (audioStreamIdx >= 0) {
		const AVCodecParameters *par = formatContext->streams[audioStreamIdx]->codecpar;
		if (!audioCodecAllowed(par->codec_id)) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: rejected audio codec id %d", par->codec_id);
			goto finish;
		}

		const AVCodec *codec = avcodec_find_decoder(par->codec_id);
		if (!codec) {
			obs_log(LOG_WARNING, "SRT_FrameReceiver: unsupported audio codec id %d", par->codec_id);
			goto finish;
		}

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
