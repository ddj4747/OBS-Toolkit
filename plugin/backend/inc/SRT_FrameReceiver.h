#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <obs-module.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

struct AVFormatContextDeleter {
	void operator()(AVFormatContext *ctx) const {
		if (ctx)
			avformat_close_input(&ctx);
	}
};

using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;

struct AVCodecContextDeleter {
	void operator()(AVCodecContext *ctx) const {
		if (ctx)
			avcodec_free_context(&ctx);
	}
};

using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

struct AVPacketDeleter {
	void operator()(AVPacket *p) const { av_packet_free(&p); }
};

using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct AVFrameDeleter {
	void operator()(AVFrame *f) const { av_frame_free(&f); }
};

using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

struct SwsContextDeleter {
	void operator()(SwsContext *c) const { sws_freeContext(c); }
};

using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

struct SwrContextDeleter {
	void operator()(SwrContext *c) const { swr_free(&c); }
};

using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

struct ConvertedAudio {
	uint8_t *data[AV_NUM_DATA_POINTERS] = {};
	int capacity = 0;
	int channels = 0;

	ConvertedAudio() = default;
	ConvertedAudio(const ConvertedAudio &) = delete;
	ConvertedAudio &operator=(const ConvertedAudio &) = delete;

	void reset();
	~ConvertedAudio();
};

struct ScaledBuffer {
	uint8_t *data[4] = {nullptr, nullptr, nullptr, nullptr};
	int lineSize[4] = {0, 0, 0, 0};
	int width = 0;
	int height = 0;
	AVPixelFormat srcFormat = AV_PIX_FMT_NONE;

	ScaledBuffer() = default;
	ScaledBuffer(const ScaledBuffer &) = delete;
	ScaledBuffer &operator=(const ScaledBuffer &) = delete;

	void reset();
	~ScaledBuffer();
};

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class SRT_FrameReceiver {
public:
	SRT_FrameReceiver() = delete;
	SRT_FrameReceiver(const SRT_FrameReceiver &) = delete;
	SRT_FrameReceiver &operator=(const SRT_FrameReceiver &) = delete;
	SRT_FrameReceiver(SRT_FrameReceiver &&) = delete;
	SRT_FrameReceiver &operator=(SRT_FrameReceiver &&) = delete;

	explicit SRT_FrameReceiver(uint16_t port);
	~SRT_FrameReceiver();

	void connectReceiver(std::function<void(obs_source_frame)> &&frameCallback,
			     std::function<void(obs_source_audio)> &&audioCallback, std::string passphrase);
	void disconnectReceiver();
	NO_DISCARD bool active() const;
	uint32_t getBitrate();

private:
	void startReceiver();
	void stopReceiver();
	void closeStream();

	void receiveThread(const std::stop_token &token);
	bool openStream();
	void submitFrame(AVFrame *frame);
	void submitAudio(AVFrame *frame);
	static bool geometryAllowed(int width, int height);

	static constexpr std::size_t c_minPassphraseLength = 10;
	static constexpr int c_maxWidth = 4096;
	static constexpr int c_maxHeight = 4096;
	static constexpr int64_t c_maxPixels = 3840LL * 2160;

	std::function<void(obs_source_frame)> m_frameCallback;
	std::function<void(obs_source_audio)> m_audioCallback;

	std::mutex m_mutex;
	std::mutex m_callbackMutex;

	std::atomic<bool> m_active{false};
	std::atomic<bool> m_interruptStop{false};
	std::jthread m_frameReceiverThread{};
	uint16_t m_port{0};
	std::string m_passphrase;

	AVCodecContextPtr m_avCodecContext;
	AVCodecContextPtr m_avAudioCodecContext;
	AVFormatContextPtr m_avFormatContext;
	SwsContextPtr m_swsContext;
	ScaledBuffer m_scaledBuffer;
	SwrContextPtr m_swrContext;
	ConvertedAudio m_convertedAudio;
	AVSampleFormat m_swrSrcFormat = AV_SAMPLE_FMT_NONE;

	int m_swrSrcRate = 0;
	int m_swrSrcChannels = 0;
	int m_videoStreamIdx = -1;
	int m_audioStreamIdx = -1;

	std::atomic<uint64_t> m_bitCount{0};
	std::atomic<uint64_t> m_bitrateLastCheck{0};
};
