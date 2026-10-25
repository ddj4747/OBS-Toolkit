#pragma once

#include <ctime>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

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

	static constexpr long CERTIFICATE_LIFETIME_SECONDS = 60L * 60 * 24 * 30;
	static constexpr time_t CERTIFICATE_MINIMAL_TIME_LEFT_SECONDS = 60 * 10;
	static constexpr int SERIAL_NUMBER_BITS = 64;

	using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;
	using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;
	using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
	using BignumPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
	using X509NamePtr = std::unique_ptr<X509_NAME, decltype(&X509_NAME_free)>;

	static std::string openSSLError();
	static std::string displayPath(const std::filesystem::path &path);
	static bool writeBioToFile(BIO *bio, const std::filesystem::path &path);
	static X509Ptr readCertificate(const std::filesystem::path &path);
	static bool setRandomSerialNumber(X509 *certificate);
	static bool setSubjectAndIssuer(X509 *certificate);
	static bool addCaBasicConstraints(X509 *certificate);
	static std::string toHex(const unsigned char *data, unsigned int length);
};
