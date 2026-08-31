#pragma once

#include <atomic>
#include <cstdint>
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

struct ScaledBuffer {
	uint8_t *data[4] = {nullptr, nullptr, nullptr, nullptr};
	int lineSize[4] = {0, 0, 0, 0};
	int width = 0;
	int height = 0;
	AVPixelFormat srcFormat = AV_PIX_FMT_NONE;

	ScaledBuffer() = default;
	ScaledBuffer(const ScaledBuffer &) = delete;
	ScaledBuffer &operator=(const ScaledBuffer &) = delete;

	void reset() {
		if (data[0])
			av_freep(&data[0]);
		data[1] = data[2] = data[3] = nullptr;
		lineSize[0] = lineSize[1] = lineSize[2] = lineSize[3] = 0;
		width = 0;
		height = 0;
		srcFormat = AV_PIX_FMT_NONE;
	}

	~ScaledBuffer() { reset(); }
};

class SRT_FrameReceiver {
public:
	SRT_FrameReceiver();

	static void init(uint16_t port);
	static void connectReceiver(std::function<void(obs_source_frame)> &&frameCallback, std::string passphrase);
	static void disconnectReceiver();
	static bool active();

private:
	void startReceiver();
	void stopReceiver();
	void closeStream();

	void receiveThread(const std::stop_token &token);
	bool openStream();
	void submitFrame(AVFrame *frame);
	static bool geometryAllowed(int width, int height);

	static SRT_FrameReceiver *s_instance;
	static std::mutex s_mutex;
	static std::atomic<bool> s_initialized;

	static constexpr std::size_t c_minPassphraseLength = 10;
	static constexpr int c_maxWidth = 4096;
	static constexpr int c_maxHeight = 4096;
	static constexpr int64_t c_maxPixels = 3840LL * 2160;

	std::function<void(obs_source_frame)> m_frameCallback;
	std::mutex m_callbackMutex;

	std::atomic<bool> m_active{false};
	std::atomic<bool> m_interruptStop{false};
	std::jthread m_frameReceiverThread{};
	uint16_t m_port{0};
	std::string m_passphrase;

	AVCodecContextPtr m_avCodecContext;
	AVFormatContextPtr m_avFormatContext;
	SwsContextPtr m_swsContext;
	ScaledBuffer m_scaledBuffer;

	int m_videoStreamIdx = -1;
};
