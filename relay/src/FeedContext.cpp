#include <FeedContext.h>

#include <QMetaObject>
#include <QThread>
#include <utility>

FeedContext::FeedContext(std::string streamKey, const StreamProtocol protocol, std::string address,
			 MediamtxManager *mediaManager, GoIRL_Process *goIrl, QObject *parent)
	: QObject(parent),
	  m_streamKey(std::move(streamKey)),
	  m_protocol(protocol),
	  m_address(std::move(address)),
	  m_mediaManager(mediaManager),
	  m_goIrl(goIrl) {
	if (m_mediaManager && m_protocol != StreamProtocol::SRTLA) {
		connect(m_mediaManager, &MediamtxManager::inputAdded, this,
			[this](const QString &name, const QString &link) {
				if (!matches(name)) {
					return;
				}

				if (m_state == State::Stopping) {
					m_mediaManager->removeInput(m_streamKey, m_protocol);
					return;
				}

				if (m_state == State::Starting) {
					m_link = link;
					m_state = State::Running;
					emit started(m_link);
				}
			});
		connect(m_mediaManager, &MediamtxManager::inputRemoved, this, [this](const QString &name) {
			if (matches(name) && m_state == State::Stopping) {
				m_state = State::Stopped;
				m_link.clear();
				emit stopped();
			}
		});
		connect(m_mediaManager, &MediamtxManager::inputError, this,
			[this](const QString &name, const QString &message) {
				if (matches(name) && m_state != State::Stopped && m_state != State::Failed) {
					fail(message);
				}
			});
		connect(m_mediaManager, &MediamtxManager::serverError, this, [this](MediamtxManager::ServerError) {
			if (m_state != State::Stopped && m_state != State::Failed) {
				fail(QStringLiteral("MediaMTX server failed"));
			}
		});
		connect(m_mediaManager, &MediamtxManager::serverStopped, this, [this] {
			if (m_state == State::Stopping) {
				m_state = State::Stopped;
				m_link.clear();
				emit stopped();
			} else if (m_state == State::Starting || m_state == State::Running) {
				fail(QStringLiteral("MediaMTX server stopped"));
			}
		});
		connect(m_mediaManager, &MediamtxManager::inputAvailable, this, [this](const QString &name) {
			if (matches(name) && m_state == State::Running) {
				emit available();
			}
		});
		connect(m_mediaManager, &MediamtxManager::inputUnavailable, this, [this](const QString &name) {
			if (matches(name) && m_state == State::Running) {
				emit unavailable();
			}
		});
	}

	if (m_goIrl && m_protocol == StreamProtocol::SRTLA) {
		connect(m_goIrl, &GoIRL_Process::serverStarted, this, [this] {
			if (m_state == State::Starting) {
				m_link = MediamtxManager::publishLink(m_streamKey, m_protocol, m_address);
				m_state = State::Running;
				emit started(m_link);
			}
		});
		connect(m_goIrl, &GoIRL_Process::serverStopped, this, [this] {
			if (m_state == State::Stopping) {
				m_state = State::Stopped;
				m_link.clear();
				emit stopped();
			}
		});
		connect(m_goIrl, &GoIRL_Process::serverError, this, [this](GoIRL_Process::ServerError) {
			if (m_state != State::Stopped && m_state != State::Failed) {
				fail(QStringLiteral("go-irl server failed"));
			}
		});
	}
}

const std::string &FeedContext::streamKey() const {
	return m_streamKey;
}

StreamProtocol FeedContext::protocol() const {
	return m_protocol;
}

FeedContext::State FeedContext::state() const {
	return m_state;
}

QString FeedContext::link() const {
	return m_link;
}

void FeedContext::start() {
	if (QThread::currentThread() != thread()) {
		QMetaObject::invokeMethod(this, [this] { start(); }, Qt::QueuedConnection);
		return;
	}

	if (m_state != State::Stopped && m_state != State::Failed) {
		return;
	}

	if (QString::fromStdString(m_streamKey).trimmed().isEmpty()) {
		fail(QStringLiteral("Stream key is empty"));
		return;
	}

	m_link.clear();
	m_state = State::Starting;

	if (m_protocol == StreamProtocol::SRTLA) {
		if (!m_goIrl || m_goIrl->thread() != thread() || m_goIrl->running()) {
			fail(QStringLiteral("go-irl is unavailable or already in use"));
			return;
		}

		m_goIrl->startServer(m_streamKey);
		return;
	}

	if (!m_mediaManager || m_mediaManager->thread() != thread()) {
		fail(QStringLiteral("MediaMTX manager is unavailable"));
		return;
	}

	if (m_protocol != StreamProtocol::SRT && m_protocol != StreamProtocol::RTSP &&
	    m_protocol != StreamProtocol::RTMP && m_protocol != StreamProtocol::WebRTC &&
	    m_protocol != StreamProtocol::MoQ) {
		fail(QStringLiteral("Unsupported protocol"));
		return;
	}

	m_mediaManager->addInput(m_streamKey, m_protocol, m_address);
}

void FeedContext::stop() {
	if (QThread::currentThread() != thread()) {
		QMetaObject::invokeMethod(this, [this] { stop(); }, Qt::QueuedConnection);
		return;
	}

	if (m_state == State::Stopped || m_state == State::Stopping) {
		return;
	}

	if (m_state == State::Failed) {
		m_state = State::Stopped;
		m_link.clear();
		emit stopped();
		return;
	}

	const State previousState = m_state;
	m_state = State::Stopping;

	if (m_protocol == StreamProtocol::SRTLA && m_goIrl) {
		m_goIrl->stopServer();
	} else if (m_protocol != StreamProtocol::SRTLA && m_mediaManager) {
		if (previousState == State::Starting && m_mediaManager->ready()) {
			return;
		}

		if (m_mediaManager->ready()) {
			m_mediaManager->removeInput(m_streamKey, m_protocol);
		} else {
			m_mediaManager->removeInput(m_streamKey, m_protocol);
			m_state = State::Stopped;
			emit stopped();
		}
	} else {
		fail(QStringLiteral("Feed manager is unavailable"));
	}
}

void FeedContext::fail(const QString &message) {
	m_state = State::Failed;
	m_link.clear();
	emit error(message);
}

bool FeedContext::matches(const QString &name) const {
	return name == QString::fromStdString(m_streamKey);
}
