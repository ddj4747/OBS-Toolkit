#pragma once

#include <filesystem>
#include <mutex>
#include <string>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class CertificateManager final {
public:
	CertificateManager() = delete;

	static bool generateCertificate(const std::filesystem::path &privateKeyPath,
					const std::filesystem::path &certificatePath);
	NO_DISCARD static bool isCertificateValid(const std::filesystem::path &certificatePath);
	NO_DISCARD static bool isPrivateKeyValid(const std::filesystem::path &privateKeyPath);
	NO_DISCARD static std::string getCertificateFingerprint(const std::filesystem::path &certificatePath);

private:
	static std::mutex s_mutex;
};
